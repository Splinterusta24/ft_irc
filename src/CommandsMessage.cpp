#include "Server.hpp"
#include "Replies.hpp"
#include "Utils.hpp"

// ---------------------------------------------------------------------------
// PRIVMSG / NOTICE
// NOTICE'e asla hata cevabı üretilmez (RFC 2812) — aksi hâlde iki sunucu/istemci
// arasında sonsuz hata döngüsü oluşabilir.
// ---------------------------------------------------------------------------
void Server::cmdPrivmsg(Client* client, Parser& parser)
{
    deliverMessage(client, parser, "PRIVMSG", false);
}

void Server::cmdNotice(Client* client, Parser& parser)
{
    deliverMessage(client, parser, "NOTICE", true);
}

void Server::deliverMessage(Client* client, Parser& parser, const std::string& type, bool silent)
{
    if (parser.paramCount() < 1)
    {
        if (!silent)
            sendNumeric(client, ERR_NORECIPIENT, ":No recipient given (" + type + ")");
        return;
    }

    const std::string text = parser.hasTrailing() ? parser.getTrailing() : parser.getParam(1);
    if (text.empty())
    {
        if (!silent)
            sendNumeric(client, ERR_NOTEXTTOSEND, ":No text to send");
        return;
    }

    std::vector<std::string> targets = Utils::split(parser.getParam(0), ',');

    for (size_t i = 0; i < targets.size(); ++i)
    {
        const std::string& target = targets[i];
        if (target.empty())
            continue;

        const std::string payload = client->getPrefix() + " " + type + " " + target + " :" + text + "\r\n";

        if (target[0] == '#' || target[0] == '&')
        {
            Channel* channel = getChannel(target);
            if (!channel)
            {
                if (!silent)
                    sendNumeric(client, ERR_NOSUCHCHANNEL, target + " :No such channel");
                continue;
            }
            if (!channel->isMember(client))
            {
                if (!silent)
                    sendNumeric(client, ERR_CANNOTSENDTOCHAN, channel->getName() + " :Cannot send to channel");
                continue;
            }
            // Gönderen kendi mesajını geri almaz; irssi zaten ekranda gösterir
            channel->broadcast(payload, client);
        }
        else
        {
            Client* receiver = getClientByNick(target);
            if (!receiver)
            {
                if (!silent)
                    sendNumeric(client, ERR_NOSUCHNICK, target + " :No such nick/channel");
                continue;
            }
            receiver->queueMessage(payload);
        }
    }
}

// ---------------------------------------------------------------------------
// WHO - irssi kanala girdikten sonra üye listesini bununla doldurur
// Format: WHO <#kanal | nick>
// ---------------------------------------------------------------------------
void Server::cmdWho(Client* client, Parser& parser)
{
    if (parser.paramCount() < 1)
    {
        sendNumeric(client, RPL_ENDOFWHO, "* :End of /WHO list");
        return;
    }

    const std::string mask = parser.getParam(0);

    if (mask[0] == '#' || mask[0] == '&')
    {
        Channel* channel = getChannel(mask);
        if (channel)
        {
            const std::vector<Client*>& members = channel->getMembers();
            for (size_t i = 0; i < members.size(); ++i)
            {
                Client* member = members[i];
                const std::string flags = channel->isOperator(member) ? "H@" : "H";
                sendNumeric(client, RPL_WHOREPLY,
                            channel->getName() + " " + member->getUsername() + " " + member->getHostname()
                            + " " + SERVER_NAME + " " + member->getNickname() + " " + flags
                            + " :0 " + member->getRealname());
            }
        }
    }
    else
    {
        Client* target = getClientByNick(mask);
        if (target)
        {
            sendNumeric(client, RPL_WHOREPLY,
                        "* " + target->getUsername() + " " + target->getHostname()
                        + " " + SERVER_NAME + " " + target->getNickname() + " H"
                        + " :0 " + target->getRealname());
        }
    }

    sendNumeric(client, RPL_ENDOFWHO, mask + " :End of /WHO list");
}

// ---------------------------------------------------------------------------
// WHOIS
// ---------------------------------------------------------------------------
void Server::cmdWhois(Client* client, Parser& parser)
{
    const std::string nick = parser.paramCount() > 0 ? parser.getParam(0) : parser.getTrailing();
    if (nick.empty())
    {
        sendNumeric(client, ERR_NONICKNAMEGIVEN, ":No nickname given");
        return;
    }

    Client* target = getClientByNick(nick);
    if (!target)
    {
        sendNumeric(client, ERR_NOSUCHNICK, nick + " :No such nick/channel");
        sendNumeric(client, RPL_ENDOFWHOIS, nick + " :End of /WHOIS list");
        return;
    }

    sendNumeric(client, RPL_WHOISUSER,
                target->getNickname() + " " + target->getUsername() + " " + target->getHostname()
                + " * :" + target->getRealname());
    sendNumeric(client, RPL_WHOISSERVER,
                target->getNickname() + " " + SERVER_NAME + " :" + SERVER_VERSION);

    // Ortak olsun olmasın, hedefin üye olduğu kanallar
    std::string channels;
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
    {
        if (!it->second->isMember(target))
            continue;
        if (!channels.empty())
            channels += " ";
        if (it->second->isOperator(target))
            channels += "@";
        channels += it->second->getName();
    }
    if (!channels.empty())
        sendNumeric(client, RPL_WHOISCHANNELS, target->getNickname() + " :" + channels);

    sendNumeric(client, RPL_ENDOFWHOIS, target->getNickname() + " :End of /WHOIS list");
}
