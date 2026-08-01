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
    std::map<int, Client*>     _clients; // fd'ye karşılık Client nesnesi
    std::map<std::string, Channel*> _channels; // Kanal adına karşılık Channel nesnesi

    // Özel metodlar: Sadece sunucu içinden çağrılır
    bool setNonBlocking(int fd);
    void acceptNewClient();
    void receiveFromClient(int fd);
    void flushClientOutput(int fd);
    void disconnectClient(int fd);

    // Komut yönlendirici
    void executeCommand(Client* client, Parser& parser);

    // Bireysel komut işleyiciler
    void cmdPass(Client* client, Parser& parser);
    void cmdNick(Client* client, Parser& parser);
    void cmdUser(Client* client, Parser& parser);
    void cmdJoin(Client* client, Parser& parser);
    void cmdPrivmsg(Client* client, Parser& parser);
    void cmdKick(Client* client, Parser& parser);
    void cmdInvite(Client* client, Parser& parser);
    void cmdTopic(Client* client, Parser& parser);
    void cmdMode(Client* client, Parser& parser);
    void cmdQuit(Client* client, Parser& parser);

    // Yardımcı fonksiyonlar
    void checkRegistration(Client* client);
    void sendNumeric(Client* client, const std::string& numeric, const std::string& message);
    Channel* getChannel(const std::string& name);
    Client* getClientByNick(const std::string& nick);

public:
    // Sunucu oluşturulurken port ve şifre atanır
    Server(int port, const std::string& password);
    ~Server();

    // Sunucu soketini oluşturur, bind ve listen işlemlerini yapar
    void init();
    
    // Sonsuz poll döngüsünü başlatır ve event'leri yönetir
    void run();
};

#endif
