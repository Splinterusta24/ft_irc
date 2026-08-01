#include "Server.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Yapıcı (Constructor): Port, şifre ve running durumunu başlatır
Server::Server(int port, const std::string& password) : _listenFd(-1), _port(port), _password(password), _running(false)
{
}

// Yıkıcı (Destructor): Kalan açık istemci ve sunucu soketlerini kapatır
Server::~Server()
{
    if (_listenFd != -1)
        close(_listenFd);
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        close(it->first);
        delete it->second;
    }
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
    {
        delete it->second;
    }
}

// Belirtilen dosya tanımlayıcısını non-blocking moda geçirir
bool Server::setNonBlocking(int fd)
{
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
    {
        std::cerr << "Error: fcntl O_NONBLOCK failed" << std::endl;
        return false;
    }
    return true;
}

// Sunucu kurulumunu yapar: soket oluştur, ayarla, bind yap ve listen moduna geç
void Server::init()
{
    _listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenFd == -1)
        throw std::runtime_error("Error: socket creation failed");

    int opt = 1;
    // Portun program kapandıktan hemen sonra kullanılabilmesi için
    if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        throw std::runtime_error("Error: setsockopt failed");

    if (!setNonBlocking(_listenFd))
        throw std::runtime_error("Error: setNonBlocking failed on listening socket");

    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(_port);

    if (bind(_listenFd, (struct sockaddr*)&address, sizeof(address)) == -1)
        throw std::runtime_error("Error: bind failed");

    if (listen(_listenFd, SOMAXCONN) == -1)
        throw std::runtime_error("Error: listen failed");

    // Ana dinleme soketini poll listemize ekliyoruz (Gelen yeni bağlantıları yakalamak için)
    struct pollfd pfd;
    pfd.fd = _listenFd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    _pollFds.push_back(pfd);

    _running = true;
    std::cout << "Server is listening on port " << _port << std::endl;
}

// Ana döngü: poll() çağırarak I/O olaylarını işler
void Server::run()
{
    while (_running)
    {
        // -1: Herhangi bir olay olana kadar süresiz bekle
        int ready = poll(_pollFds.data(), _pollFds.size(), -1);
        
        if (ready < 0)
        {
            std::cerr << "Error: poll failed" << std::endl;
            break;
        }

        // vector içinde iterasyon yaparken listeyi değiştirmemek için indeksi dikkatli yönetmeliyiz
        for (size_t i = 0; i < _pollFds.size(); ++i)
        {
            // Eğer o anki sokette herhangi bir olay yoksa bir sonrakine geç
            if (_pollFds[i].revents == 0)
                continue;

            int currentFd = _pollFds[i].fd;

            // Hata veya bağlantı kopma olayları
            if (_pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                disconnectClient(currentFd);
                i--; // Vektörden eleman sildiğimiz için indeksi geri alıyoruz
                continue;
            }

            // Okuma (veri veya yeni bağlantı) hazır durumu
            if (_pollFds[i].revents & POLLIN)
            {
                if (currentFd == _listenFd)
                {
                    acceptNewClient();
                }
                else
                {
                    receiveFromClient(currentFd);
                }
            }

            // Eğer silinmemişse ve yazmaya hazırsa (output buffer'da veri varsa)
            // Not: _clients haritasında kontrol yapıyoruz çünkü üstteki receive sırasında silinmiş olabilir.
            if (i < _pollFds.size() && _pollFds[i].fd == currentFd && (_pollFds[i].revents & POLLOUT))
            {
                flushClientOutput(currentFd);
            }
        }
    }
}

// Ana sokete gelen yeni müşteri bağlantısını kabul eder ve listeye ekler
void Server::acceptNewClient()
{
    struct sockaddr_in clientAddress;
    socklen_t addrLen = sizeof(clientAddress);
    
    int clientFd = accept(_listenFd, (struct sockaddr*)&clientAddress, &addrLen);
    if (clientFd == -1)
    {
        std::cerr << "Error: accept failed" << std::endl;
        return;
    }

    if (!setNonBlocking(clientFd))
    {
        close(clientFd);
        return;
    }

    // Yeni Client nesnesi oluştur ve haritaya ekle
    _clients[clientFd] = new Client(clientFd);

    // Yeni soketi poll dinleme listesine ekle
    struct pollfd pfd;
    pfd.fd = clientFd;
    pfd.events = POLLIN; // Başlangıçta sadece ondan gelecekleri okumayı dinliyoruz
    pfd.revents = 0;
    _pollFds.push_back(pfd);

    std::cout << "New client connected. FD: " << clientFd << std::endl;
}

// İstemciden veri okur ve client buffer'ına ekler
void Server::receiveFromClient(int fd)
{
    char buffer[1024];
    std::memset(buffer, 0, sizeof(buffer));

    int bytesRead = recv(fd, buffer, sizeof(buffer) - 1, 0);

    if (bytesRead <= 0)
    {
        // 0 bağlantının koptuğunu, -1 ise hatayı gösterir
        disconnectClient(fd);
    }
    else
    {
        // Okunan veriyi client buffer'ına gönder
        Client* client = _clients[fd];
        client->appendInput(buffer);

        // Satır satır (komut komut) işle
        std::string line;
        while ((line = client->extractCommand()) != "")
        {
            std::cout << "Received command from FD " << fd << ": " << line << std::endl;
            
            Parser parser;
            if (parser.parse(line))
            {
                executeCommand(client, parser);
            }
            
            // Output buffer'a veri eklemiş olabiliriz, bu yüzden POLLOUT yetkisi verelim
            if (client->hasOutput())
            {
                for (size_t i = 0; i < _pollFds.size(); ++i)
                {
                    if (_pollFds[i].fd == fd)
                    {
                        _pollFds[i].events |= POLLOUT;
                        break;
                    }
                }
            }
        }
    }
}

// İstemcinin output buffer'ındaki veriyi gönderir (kısmi göndermelere de destek verir)
void Server::flushClientOutput(int fd)
{
    if (_clients.find(fd) == _clients.end())
        return;

    Client* client = _clients[fd];
    std::string data = client->getOutputBuffer();

    if (data.empty())
    {
        // Gönderilecek bir şey kalmadıysa POLLOUT dinlemesini kaldır (Sadece POLLIN kalsın)
        for (size_t i = 0; i < _pollFds.size(); ++i)
        {
            if (_pollFds[i].fd == fd)
            {
                _pollFds[i].events = POLLIN;
                break;
            }
        }
        return;
    }

    int bytesSent = send(fd, data.c_str(), data.length(), 0);
    if (bytesSent > 0)
    {
        // Gönderilen kadarını buffer'dan temizle
        client->clearOutput(bytesSent);
    }
    else
    {
        // Hata durumunda (isteğe bağlı olarak) bağlantı kesilebilir
        std::cerr << "Error sending data to FD " << fd << std::endl;
    }
}

// İstemcinin bağlantısını tamamen keser ve bellekten siler
void Server::disconnectClient(int fd)
{
    if (_clients.find(fd) == _clients.end()) return;
    
    std::cout << "Client disconnected. FD: " << fd << std::endl;
    Client* client = _clients[fd];

    // İstemciyi bulunduğu tüm kanallardan çıkar
    std::vector<std::string> emptyChannels;
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
    {
        Channel* channel = it->second;
        if (channel->isMember(client))
        {
            channel->removeMember(client);
            channel->broadcast(":" + client->getNickname() + " QUIT :Client disconnected\r\n");
            if (channel->getMemberCount() == 0)
                emptyChannels.push_back(it->first);
        }
    }

    // Boşalan kanalları sil
    for (size_t i = 0; i < emptyChannels.size(); ++i)
    {
        delete _channels[emptyChannels[i]];
        _channels.erase(emptyChannels[i]);
    }

    // Vektörden pollfd kaydını sil
    for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it)
    {
        if (it->fd == fd)
        {
            _pollFds.erase(it);
            break;
        }
    }

    // Haritadan client nesnesini sil ve belleği temizle
    delete client;
    _clients.erase(fd);
    close(fd);
}
