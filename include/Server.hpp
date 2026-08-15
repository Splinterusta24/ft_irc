#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <poll.h>
#include "Client.hpp"
#include "Channel.hpp"
#include "Parser.hpp"

class Server
{
private:
    int                        _listenFd;
    int                        _port;
    std::string                _password;
    bool                       _running;
    std::vector<struct pollfd> _pollFds;
    std::map<int, Client*>     _clients;   // fd -> Client
    // Kanal adları büyük-küçük harf duyarsızdır: anahtar toIrcLower(name)
    std::map<std::string, Channel*> _channels;
    std::vector<int>           _pendingDisconnects;

    Server(const Server& other);
    Server& operator=(const Server& other);

    // --- Soket / döngü yönetimi (Server.cpp) ---
    bool setNonBlocking(int fd);
    void updatePollEvents();
    void acceptNewClient();
    void receiveFromClient(int fd);
    void flushClientOutput(int fd);
    void queueDisconnect(int fd, const std::string& reason);
    void dropClient(int fd, const std::string& reason);
    void processDisconnects();
    void removeClient(int fd);
    void removePollFd(int fd);

    // --- Yardımcılar (ServerUtils.cpp) ---
    Client*  getClientByFd(int fd);
    Client*  getClientByNick(const std::string& nick);
    Channel* getChannel(const std::string& name);
    void     createChannel(const std::string& name, Client* founder);
    void     removeChannelIfEmpty(Channel* channel);

    void sendNumeric(Client* client, const std::string& numeric, const std::string& message);
    void sendRaw(Client* client, const std::string& message);
    void sendWelcome(Client* client);
    void sendMotd(Client* client);
    void sendNames(Client* client, Channel* channel);
    void checkRegistration(Client* client);
    // Kullanıcı ile aynı kanalları paylaşan herkese (ve isteğe bağlı kendisine) yollar
    void broadcastToPeers(Client* client, const std::string& message, bool includeSelf);

    // --- Komut yönlendirici (ServerUtils.cpp) ---
    void executeCommand(Client* client, Parser& parser);

    // --- Kayıt/oturum komutları (CommandsAuth.cpp) ---
    void cmdCap(Client* client, Parser& parser);
    void cmdPass(Client* client, Parser& parser);
    void cmdNick(Client* client, Parser& parser);
    void cmdUser(Client* client, Parser& parser);
    void cmdPing(Client* client, Parser& parser);
    void cmdPong(Client* client, Parser& parser);
    void cmdQuit(Client* client, Parser& parser);
    void cmdMotd(Client* client, Parser& parser);

    // --- Kanal komutları (CommandsChannel.cpp) ---
    void cmdJoin(Client* client, Parser& parser);
    void cmdPart(Client* client, Parser& parser);
    void cmdTopic(Client* client, Parser& parser);
    void cmdKick(Client* client, Parser& parser);
    void cmdInvite(Client* client, Parser& parser);
    void cmdNames(Client* client, Parser& parser);
    void joinSingleChannel(Client* client, const std::string& name, const std::string& key);
    void partSingleChannel(Client* client, const std::string& name, const std::string& reason);

    // --- MODE (CommandsMode.cpp) ---
    void cmdMode(Client* client, Parser& parser);
    void applyChannelModes(Client* client, Channel* channel, Parser& parser);
    void showChannelModes(Client* client, Channel* channel);
    void handleUserMode(Client* client, Parser& parser);

    // --- Mesajlaşma / sorgu komutları (CommandsMessage.cpp) ---
    void cmdPrivmsg(Client* client, Parser& parser);
    void cmdNotice(Client* client, Parser& parser);
    void deliverMessage(Client* client, Parser& parser, const std::string& type, bool silent);
    void cmdWho(Client* client, Parser& parser);
    void cmdWhois(Client* client, Parser& parser);

public:
    Server(int port, const std::string& password);
    ~Server();

    // Sunucu soketini oluşturur, bind ve listen işlemlerini yapar
    void init();

    // poll() döngüsünü başlatır ve event'leri yönetir
    void run();

    // SIGINT gibi sinyallerde döngüyü sonlandırmak için
    void stop();
};

#endif
