#include "Server.hpp"
#include "Replies.hpp"
#include "Utils.hpp"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <stdexcept>
#include <set>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// main.cpp'de tanımlanır; SIGINT/SIGTERM geldiğinde 1 olur.
extern volatile sig_atomic_t g_shutdownRequested;

Server::Server(int port, const std::string& password)
    : _listenFd(-1), _port(port), _password(password), _running(false)
{
}

Server::~Server()
{
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        close(it->first);
        delete it->second;
    }
    _clients.clear();

    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        delete it->second;
    _channels.clear();

    if (_listenFd != -1)
        close(_listenFd);
}

void Server::stop()
{
    _running = false;
}

bool Server::setNonBlocking(int fd)
{
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
    {
        std::cerr << "Error: fcntl O_NONBLOCK failed" << std::endl;
        return false;
    }
    return true;
}

// Soket oluştur, ayarla, bind yap ve listen moduna geç
void Server::init()
{
    // Kapanmış bir sokete send() yapıldığında süreç SIGPIPE ile ölmesin.
    signal(SIGPIPE, SIG_IGN);

    _listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenFd == -1)
        throw std::runtime_error("Error: socket creation failed");

    int opt = 1;
    if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        throw std::runtime_error("Error: setsockopt failed");

    if (!setNonBlocking(_listenFd))
        throw std::runtime_error("Error: setNonBlocking failed on listening socket");

    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(_port));

    if (bind(_listenFd, (struct sockaddr*)&address, sizeof(address)) == -1)
        throw std::runtime_error("Error: bind failed (port already in use?)");

    if (listen(_listenFd, SOMAXCONN) == -1)
        throw std::runtime_error("Error: listen failed");

    struct pollfd pfd;
    pfd.fd = _listenFd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    _pollFds.push_back(pfd);

    _running = true;
    std::cout << SERVER_NAME << " is listening on port " << _port << std::endl;
}

// Her turda: yazılacak verisi olan soketler için POLLOUT dinlenir, yoksa dinlenmez.
void Server::updatePollEvents()
{
    for (size_t i = 0; i < _pollFds.size(); ++i)
    {
        if (_pollFds[i].fd == _listenFd)
        {
            _pollFds[i].events = POLLIN;
            continue;
        }
        short events = POLLIN;
        std::map<int, Client*>::iterator it = _clients.find(_pollFds[i].fd);
        if (it != _clients.end() && it->second->hasOutput())
            events |= POLLOUT;
        _pollFds[i].events = events;
    }
}

// Ana döngü
void Server::run()
{
    while (_running && !g_shutdownRequested)
    {
        updatePollEvents();

        int ready = poll(&_pollFds[0], _pollFds.size(), -1);
        if (ready < 0)
        {
            if (errno == EINTR)
                continue; // Sinyal ile kesildi, döngü koşulunu yeniden değerlendir
            std::cerr << "Error: poll failed: " << std::strerror(errno) << std::endl;
            break;
        }

        // Not: Bu döngü içinde _pollFds'ten eleman SİLİNMEZ. Kopan bağlantılar
        // işaretlenir ve döngü bittikten sonra processDisconnects() temizler.
        // Böylece indeks kayması ve dangling pointer sorunları oluşmaz.
        for (size_t i = 0; i < _pollFds.size(); ++i)
        {
            short revents = _pollFds[i].revents;
            if (revents == 0)
                continue;

            int fd = _pollFds[i].fd;

            if (fd == _listenFd)
            {
                if (revents & POLLIN)
                    acceptNewClient();
                continue;
            }

            if (revents & (POLLERR | POLLNVAL))
            {
                queueDisconnect(fd, "Connection error");
                continue;
            }

            if (revents & POLLIN)
                receiveFromClient(fd);

            if (revents & POLLOUT)
                flushClientOutput(fd);

            if (revents & POLLHUP)
                queueDisconnect(fd, "Connection reset by peer");
        }

        processDisconnects();
    }

    std::cout << "\nServer shutting down..." << std::endl;
}

void Server::acceptNewClient()
{
    struct sockaddr_in clientAddress;
    socklen_t addrLen = sizeof(clientAddress);
    std::memset(&clientAddress, 0, sizeof(clientAddress));

    int clientFd = accept(_listenFd, (struct sockaddr*)&clientAddress, &addrLen);
    if (clientFd == -1)
        return; // EAGAIN/EWOULDBLOCK dahil: bir sonraki poll turunda tekrar denenir

    if (!setNonBlocking(clientFd))
    {
        close(clientFd);
        return;
    }

    const char* ip = inet_ntoa(clientAddress.sin_addr);
    std::string host = ip ? std::string(ip) : std::string("localhost");

    _clients[clientFd] = new Client(clientFd, host);

    struct pollfd pfd;
    pfd.fd = clientFd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    _pollFds.push_back(pfd);

    std::cout << "[+] New connection from " << host << " (fd " << clientFd << ")" << std::endl;
}

void Server::receiveFromClient(int fd)
{
    Client* client = getClientByFd(fd);
    if (!client || client->isMarkedForQuit())
        return;

    char buffer[RECV_BUFFER];
    ssize_t bytesRead = recv(fd, buffer, sizeof(buffer), 0);

    if (bytesRead == 0)
    {
        queueDisconnect(fd, "Connection closed");
        return;
    }
    if (bytesRead < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return;
        queueDisconnect(fd, "Read error");
        return;
    }

    // Binary-safe: recv'in döndürdüğü uzunluk kullanılır ('\0' içeren veri kesilmez)
    client->appendInput(std::string(buffer, static_cast<size_t>(bytesRead)));

    // Satır sonu göndermeden sınırsız veri yığan istemciye karşı koruma
    if (client->getInputSize() > MAX_INPUT_SIZE)
    {
        client->clearInput();
        sendRaw(client, "ERROR :Input line was too long\r\n");
        queueDisconnect(fd, "Input line too long");
        return;
    }

    std::string line;
    while (!client->isMarkedForQuit() && client->extractCommand(line))
    {
        if (line.empty())
            continue; // Boş satır: yok say, kalan komutları işlemeye devam et

        std::cout << "    <- [fd " << fd << "] " << line << std::endl;

        Parser parser;
        if (parser.parse(line))
            executeCommand(client, parser);
    }
}

void Server::flushClientOutput(int fd)
{
    Client* client = getClientByFd(fd);
    if (!client || !client->hasOutput())
        return;

    std::string data = client->getOutputBuffer();
    ssize_t bytesSent = send(fd, data.c_str(), data.length(), 0);

    if (bytesSent > 0)
    {
        client->clearOutput(static_cast<size_t>(bytesSent));
    }
    else if (bytesSent < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
    {
        queueDisconnect(fd, "Write error");
    }
}

// Bağlantıyı hemen kapatmaz; sadece işaretler. Gerçek temizlik processDisconnects()'te.
void Server::queueDisconnect(int fd, const std::string& reason)
{
    Client* client = getClientByFd(fd);
    if (!client || client->isMarkedForQuit())
        return;

    client->markForQuit(reason);
    _pendingDisconnects.push_back(fd);
}

void Server::processDisconnects()
{
    if (_pendingDisconnects.empty())
        return;

    std::vector<int> pending = _pendingDisconnects;
    _pendingDisconnects.clear();

    for (size_t i = 0; i < pending.size(); ++i)
        removeClient(pending[i]);
}

void Server::removeClient(int fd)
{
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end())
        return;

    Client* client = it->second;
    std::string reason = client->getQuitReason().empty() ? "Client exited" : client->getQuitReason();

    // Aynı kanallardaki herkese tek bir QUIT mesajı gönder
    if (client->isRegistered())
        broadcastToPeers(client, client->getPrefix() + " QUIT :" + reason + "\r\n", false);

    // Kanallardan çıkar, boşalanları sil
    std::vector<std::string> emptyChannels;
    for (std::map<std::string, Channel*>::iterator ch = _channels.begin(); ch != _channels.end(); ++ch)
    {
        ch->second->removeMember(client);
        if (ch->second->isEmpty())
            emptyChannels.push_back(ch->first);
    }
    for (size_t i = 0; i < emptyChannels.size(); ++i)
    {
        delete _channels[emptyChannels[i]];
        _channels.erase(emptyChannels[i]);
    }

    // Kalan çıktıyı (örn. "ERROR :...") göndermek için son bir deneme
    if (client->hasOutput())
    {
        std::string data = client->getOutputBuffer();
        send(fd, data.c_str(), data.length(), 0);
    }

    std::cout << "[-] Disconnected fd " << fd
              << " (" << (client->getNickname().empty() ? "*" : client->getNickname())
              << ") : " << reason << std::endl;

    removePollFd(fd);
    _clients.erase(it);
    delete client;
    close(fd);
}

void Server::removePollFd(int fd)
{
    for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it)
    {
        if (it->fd == fd)
        {
            _pollFds.erase(it);
            return;
        }
    }
}
