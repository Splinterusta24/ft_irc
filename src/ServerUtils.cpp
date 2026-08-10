#include "Server.hpp"
#include "Replies.hpp"
#include "Utils.hpp"
#include <set>

// ---------------------------------------------------------------------------
// Arama yardımcıları
// ---------------------------------------------------------------------------

Client* Server::getClientByFd(int fd)
{
    std::map<int, Client*>::iterator it = _clients.find(fd);
    return (it == _clients.end()) ? NULL : it->second;
}

Client* Server::getClientByNick(const std::string& nick)
{
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        if (Utils::ircEquals(it->second->getNickname(), nick))
            return it->second;
    }
    return NULL;
}

Channel* Server::getChannel(const std::string& name)
{
    std::map<std::string, Channel*>::iterator it = _channels.find(Utils::toIrcLower(name));
    return (it == _channels.end()) ? NULL : it->second;
}

void Server::createChannel(const std::string& name, Client* founder)
{
    Channel* channel = new Channel(name);
    _channels[Utils::toIrcLower(name)] = channel;
    channel->addMember(founder);
    channel->addOperator(founder); // Kanalı ilk açan kişi otomatik operatör olur
}

void Server::removeChannelIfEmpty(Channel* channel)
{
    if (!channel || !channel->isEmpty())
        return;

    std::string key = Utils::toIrcLower(channel->getName());
    _channels.erase(key);
    delete channel;
}

// ---------------------------------------------------------------------------
// Mesaj gönderme yardımcıları
// ---------------------------------------------------------------------------

void Server::sendRaw(Client* client, const std::string& message)
{
    if (client)
        client->queueMessage(message);
}

// Numerik cevap formatı: ":<server> <code> <nick> <mesaj>\r\n"
// Kayıt tamamlanmadan nick boş olabilir; bu durumda RFC gereği "*" kullanılır.
void Server::sendNumeric(Client* client, const std::string& numeric, const std::string& message)
{
    if (!client)
        return;
    std::string nick = client->getNickname().empty() ? "*" : client->getNickname();
    client->queueMessage(":" + std::string(SERVER_NAME) + " " + numeric + " " + nick + " " + message + "\r\n");
}

// Kullanıcının bulunduğu kanallardaki herkese (her kişiye bir kez) mesaj gönderir.
void Server::broadcastToPeers(Client* client, const std::string& message, bool includeSelf)
{
    std::set<Client*> receivers;

    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
    {
        Channel* channel = it->second;
        if (!channel->isMember(client))
            continue;

        const std::vector<Client*>& members = channel->getMembers();
        for (size_t i = 0; i < members.size(); ++i)
        {
            if (members[i] != client)
                receivers.insert(members[i]);
        }
    }

    if (includeSelf)
        receivers.insert(client);

    for (std::set<Client*>::iterator it = receivers.begin(); it != receivers.end(); ++it)
        (*it)->queueMessage(message);
}

// ---------------------------------------------------------------------------
// Kayıt (registration) akışı
// ---------------------------------------------------------------------------

// irssi 001 numerik cevabını görene kadar bağlantıyı "kurulmuş" saymaz.
// CAP görüşmesi sürüyorsa (CAP LS gönderildi, CAP END gelmedi) beklenir.
void Server::checkRegistration(Client* client)
{
    if (client->isRegistered())
        return;
    if (client->isCapNegotiating())
        return;
    if (client->getNickname().empty() || client->getUsername().empty())
        return;

    if (!client->isPasswordAccepted())
    {
        sendNumeric(client, ERR_PASSWDMISMATCH, ":Password incorrect");
        sendRaw(client, "ERROR :Closing link: (Bad password)\r\n");
        queueDisconnect(client->getFd(), "Bad password");
        return;
    }

    client->setRegistered(true);
    sendWelcome(client);
}

void Server::sendWelcome(Client* client)
{
    const std::string nick = client->getNickname();

    sendNumeric(client, RPL_WELCOME,
                ":Welcome to the ft_irc Network, " + nick + "!" + client->getUsername() + "@" + client->getHostname());
    sendNumeric(client, RPL_YOURHOST,
                ":Your host is " + std::string(SERVER_NAME) + ", running version " + SERVER_VERSION);
    sendNumeric(client, RPL_CREATED, ":This server was created at boot time");
    sendNumeric(client, RPL_MYINFO,
                std::string(SERVER_NAME) + " " + SERVER_VERSION + " " + USER_MODES + " " + CHANNEL_MODES);
    sendNumeric(client, RPL_ISUPPORT,
                "CHANTYPES=#& CHANMODES=,k,l,it PREFIX=(o)@ NICKLEN=30 CHANNELLEN=50 "
                "TOPICLEN=390 CASEMAPPING=rfc1459 :are supported by this server");

    sendMotd(client);
}

void Server::sendMotd(Client* client)
{
    sendNumeric(client, RPL_MOTDSTART, ":- " + std::string(SERVER_NAME) + " Message of the Day -");
    sendNumeric(client, RPL_MOTD, ":- Welcome to ft_irc.");
    sendNumeric(client, RPL_MOTD, ":- Supported channel modes: +i +t +k +o +l");
    sendNumeric(client, RPL_MOTD, ":- Have fun!");
    sendNumeric(client, RPL_ENDOFMOTD, ":End of /MOTD command.");
}

void Server::sendNames(Client* client, Channel* channel)
{
    sendNumeric(client, RPL_NAMREPLY, "= " + channel->getName() + " :" + channel->getNamesList());
    sendNumeric(client, RPL_ENDOFNAMES, channel->getName() + " :End of /NAMES list");
}

// ---------------------------------------------------------------------------
// Komut yönlendirici
// ---------------------------------------------------------------------------

void Server::executeCommand(Client* client, Parser& parser)
{
    const std::string cmd = parser.getCommand();

    // Kayıt öncesinde de kabul edilen komutlar
    if (cmd == "CAP")   { cmdCap(client, parser);  return; }
    if (cmd == "PASS")  { cmdPass(client, parser); return; }
    if (cmd == "NICK")  { cmdNick(client, parser); return; }
    if (cmd == "USER")  { cmdUser(client, parser); return; }
    if (cmd == "QUIT")  { cmdQuit(client, parser); return; }
    if (cmd == "PING")  { cmdPing(client, parser); return; }
    if (cmd == "PONG")  { cmdPong(client, parser); return; }

    if (!client->isRegistered())
    {
        sendNumeric(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }

    if (cmd == "JOIN")         cmdJoin(client, parser);
    else if (cmd == "PART")    cmdPart(client, parser);
    else if (cmd == "PRIVMSG") cmdPrivmsg(client, parser);
    else if (cmd == "NOTICE")  cmdNotice(client, parser);
    else if (cmd == "KICK")    cmdKick(client, parser);
    else if (cmd == "INVITE")  cmdInvite(client, parser);
    else if (cmd == "TOPIC")   cmdTopic(client, parser);
    else if (cmd == "MODE")    cmdMode(client, parser);
    else if (cmd == "NAMES")   cmdNames(client, parser);
    else if (cmd == "WHO")     cmdWho(client, parser);
    else if (cmd == "WHOIS")   cmdWhois(client, parser);
    else if (cmd == "MOTD")    cmdMotd(client, parser);
    else
        sendNumeric(client, ERR_UNKNOWNCOMMAND, cmd + " :Unknown command");
}
