#include "../include/Server.hpp"
#include <iostream>

void Server::sendNumeric(Client* client, const std::string& numeric, const std::string& message)
{
    std::string reply = ":" + std::string("ircserv") + " " + numeric + " " + client->getNickname() + " " + message + "\r\n";
    client->queueMessage(reply);
}

Channel* Server::getChannel(const std::string& name)
{
    if (_channels.find(name) != _channels.end())
        return _channels[name];
    return NULL;
}

Client* Server::getClientByNick(const std::string& nick)
{
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        if (it->second->getNickname() == nick)
            return it->second;
    }
    return NULL;
}

void Server::checkRegistration(Client* client)
{
    if (!client->isRegistered() && client->isPasswordAccepted() && 
        !client->getNickname().empty() && !client->getUsername().empty())
    {
        client->setRegistered(true);
        sendNumeric(client, "001", ":Welcome to the Internet Relay Network " + client->getNickname());
    }
}

void Server::executeCommand(Client* client, Parser& parser)
{
    std::string cmd = parser.getCommand();
    
    if (cmd == "PASS") cmdPass(client, parser);
    else if (cmd == "NICK") cmdNick(client, parser);
    else if (cmd == "USER") cmdUser(client, parser);
    else if (!client->isRegistered()) return; // Kayıtlı olmayanlar diğer komutları kullanamaz
    else if (cmd == "JOIN") cmdJoin(client, parser);
    else if (cmd == "PRIVMSG") cmdPrivmsg(client, parser);
    else if (cmd == "KICK") cmdKick(client, parser);
    else if (cmd == "INVITE") cmdInvite(client, parser);
    else if (cmd == "TOPIC") cmdTopic(client, parser);
    else if (cmd == "MODE") cmdMode(client, parser);
    else if (cmd == "QUIT") cmdQuit(client, parser);
}

void Server::cmdPass(Client* client, Parser& parser)
{
    if (client->isRegistered()) return; // Zaten kayıtlı
    if (parser.getParams().empty()) return; // Parametre yok
    if (parser.getParams()[0] == _password)
        client->setPasswordAccepted(true);
}

void Server::cmdNick(Client* client, Parser& parser)
{
    if (parser.getParams().empty()) return;
    std::string nick = parser.getParams()[0];
    
    if (getClientByNick(nick) != NULL)
    {
        sendNumeric(client, "433", nick + " :Nickname is already in use");
        return;
    }
    client->setNickname(nick);
    checkRegistration(client);
}

void Server::cmdUser(Client* client, Parser& parser)
{
    if (client->isRegistered()) return;
    if (parser.getParams().size() < 3 || parser.getTrailing().empty()) return;
    
    client->setUsername(parser.getParams()[0]);
    client->setRealname(parser.getTrailing());
    checkRegistration(client);
}

void Server::cmdJoin(Client* client, Parser& parser)
{
    if (parser.getParams().empty()) return;
    std::string channelName = parser.getParams()[0];
    
    Channel* channel = getChannel(channelName);
    if (!channel)
    {
        channel = new Channel(channelName);
        _channels[channelName] = channel;
        channel->addOperator(client);
    }
    
    if (channel->isInviteOnly() && !channel->isInvited(client))
    {
        sendNumeric(client, "473", channelName + " :Cannot join channel (+i)");
        return;
    }
    
    channel->addMember(client);
    channel->broadcast(":" + client->getNickname() + " JOIN :" + channelName + "\r\n");
    
    if (!channel->getTopic().empty())
        sendNumeric(client, "332", channelName + " :" + channel->getTopic());
    
    sendNumeric(client, "353", "= " + channelName + " :" + channel->getNamesList());
    sendNumeric(client, "366", channelName + " :End of /NAMES list");
}

void Server::cmdPrivmsg(Client* client, Parser& parser)
{
    if (parser.getParams().empty() || parser.getTrailing().empty()) return;
    
    std::string target = parser.getParams()[0];
    std::string message = parser.getTrailing();
    std::string formattedMsg = ":" + client->getNickname() + " PRIVMSG " + target + " :" + message + "\r\n";
    
    if (target[0] == '#') // Kanal
    {
        Channel* channel = getChannel(target);
        if (channel && channel->isMember(client))
            channel->broadcast(formattedMsg, client);
    }
    else // Özel mesaj
    {
        Client* targetClient = getClientByNick(target);
        if (targetClient)
            targetClient->queueMessage(formattedMsg);
    }
}

void Server::cmdKick(Client* client, Parser& parser)
{
    if (parser.getParams().size() < 2) return;
    std::string channelName = parser.getParams()[0];
    std::string targetNick = parser.getParams()[1];
    std::string reason = parser.getTrailing().empty() ? "No reason" : parser.getTrailing();
    
    Channel* channel = getChannel(channelName);
    if (!channel) return;
    if (!channel->isOperator(client))
    {
        sendNumeric(client, "482", channelName + " :You're not channel operator");
        return;
    }
    
    Client* targetClient = getClientByNick(targetNick);
    if (targetClient && channel->isMember(targetClient))
    {
        channel->broadcast(":" + client->getNickname() + " KICK " + channelName + " " + targetNick + " :" + reason + "\r\n");
        channel->removeMember(targetClient);
    }
}

void Server::cmdInvite(Client* client, Parser& parser)
{
    if (parser.getParams().size() < 2) return;
    std::string targetNick = parser.getParams()[0];
    std::string channelName = parser.getParams()[1];
    
    Channel* channel = getChannel(channelName);
    if (!channel || !channel->isOperator(client)) return;
    
    Client* targetClient = getClientByNick(targetNick);
    if (targetClient)
    {
        channel->addInvite(targetClient);
        targetClient->queueMessage(":" + client->getNickname() + " INVITE " + targetNick + " :" + channelName + "\r\n");
    }
}

void Server::cmdTopic(Client* client, Parser& parser)
{
    if (parser.getParams().empty()) return;
    std::string channelName = parser.getParams()[0];
    Channel* channel = getChannel(channelName);
    if (!channel || !channel->isMember(client)) return;
    
    if (parser.getTrailing().empty() && parser.getParams().size() == 1) // Görüntüleme
    {
        if (channel->getTopic().empty())
            sendNumeric(client, "331", channelName + " :No topic is set");
        else
            sendNumeric(client, "332", channelName + " :" + channel->getTopic());
    }
    else // Değiştirme
    {
        if (channel->isTopicRestricted() && !channel->isOperator(client))
        {
            sendNumeric(client, "482", channelName + " :You're not channel operator");
            return;
        }
        channel->setTopic(parser.getTrailing());
        channel->broadcast(":" + client->getNickname() + " TOPIC " + channelName + " :" + channel->getTopic() + "\r\n");
    }
}

void Server::cmdMode(Client* client, Parser& parser)
{
    if (parser.getParams().size() < 2) return;
    std::string channelName = parser.getParams()[0];
    std::string modeString = parser.getParams()[1];
    
    Channel* channel = getChannel(channelName);
    if (!channel || !channel->isOperator(client)) return;
    
    bool add = (modeString[0] == '+');
    for (size_t i = 1; i < modeString.length(); ++i)
    {
        char m = modeString[i];
        if (m == 'i') channel->setInviteOnly(add);
        else if (m == 't') channel->setTopicRestricted(add);
    }
    channel->broadcast(":" + client->getNickname() + " MODE " + channelName + " " + modeString + "\r\n");
}

void Server::cmdQuit(Client* client, Parser& parser)
{
    (void)parser;
    disconnectClient(client->getFd());
}
