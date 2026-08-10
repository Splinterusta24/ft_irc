#include "Server.hpp"
#include "Replies.hpp"
#include "Utils.hpp"
#include <cstdlib>

// ---------------------------------------------------------------------------
// MODE - Kanal ve kullanıcı modları
// Desteklenen kanal modları: +i (davetle), +t (konu kilidi), +k (anahtar),
//                            +o (operatör), +l (kullanıcı limiti)
// irssi bir kanala girdikten hemen sonra "MODE #kanal" sorgusu gönderir;
// cevapsız kalırsa kanal başlığında mod bilgisi görünmez.
// ---------------------------------------------------------------------------
void Server::cmdMode(Client* client, Parser& parser)
{
    if (parser.paramCount() < 1)
    {
        sendNumeric(client, ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
        return;
    }

    const std::string target = parser.getParam(0);

    // Kanal değilse kullanıcı modu isteğidir
    if (target[0] != '#' && target[0] != '&')
    {
        handleUserMode(client, parser);
        return;
    }

    Channel* channel = getChannel(target);
    if (!channel)
    {
        sendNumeric(client, ERR_NOSUCHCHANNEL, target + " :No such channel");
        return;
    }

    // Parametre yoksa sorgudur
    if (parser.paramCount() < 2)
    {
        showChannelModes(client, channel);
        return;
    }

    // irssi "/mode #kanal b" ile ban listesi ister; ban desteklemiyoruz,
    // boş liste sonu göndererek istemciyi bekletmiyoruz.
    if (parser.getParam(1) == "b" || parser.getParam(1) == "+b")
    {
        sendNumeric(client, "368", channel->getName() + " :End of channel ban list");
        return;
    }

    if (!channel->isMember(client))
    {
        sendNumeric(client, ERR_NOTONCHANNEL, channel->getName() + " :You're not on that channel");
        return;
    }
    if (!channel->isOperator(client))
    {
        sendNumeric(client, ERR_CHANOPRIVSNEEDED, channel->getName() + " :You're not channel operator");
        return;
    }

    applyChannelModes(client, channel, parser);
}

void Server::showChannelModes(Client* client, Channel* channel)
{
    // Anahtar/limit değerleri yalnızca kanal üyelerine gösterilir
    const bool member = channel->isMember(client);
    sendNumeric(client, RPL_CHANNELMODEIS, channel->getName() + " " + channel->getModeString(member));
}

void Server::applyChannelModes(Client* client, Channel* channel, Parser& parser)
{
    const std::string modeString = parser.getParam(1);
    size_t paramIndex = 2; // Mod parametreleri buradan itibaren okunur

    bool adding = true;
    std::string appliedModes;   // Yayınlanacak mod dizisi, örn: "+ok"
    std::string appliedParams;  // Ona eşlik eden parametreler
    char lastSign = 0;

    for (size_t i = 0; i < modeString.length(); ++i)
    {
        const char c = modeString[i];

        if (c == '+' || c == '-')
        {
            adding = (c == '+');
            continue;
        }

        // Parametre gerektiren modlar: +k, +o, -o, +l (-k ve -l parametresizdir)
        const bool needsParam = (c == 'k' && adding) || (c == 'o') || (c == 'l' && adding);
        std::string param;
        if (needsParam)
        {
            if (paramIndex >= parser.paramCount())
            {
                if (parser.hasTrailing() && paramIndex == parser.paramCount())
                    param = parser.getTrailing();
                else
                {
                    sendNumeric(client, ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
                    continue;
                }
            }
            else
            {
                param = parser.getParam(paramIndex);
            }
            ++paramIndex;
            if (param.empty())
                continue;
        }

        bool changed = false;

        switch (c)
        {
            case 'i':
                channel->setInviteOnly(adding);
                changed = true;
                break;

            case 't':
                channel->setTopicRestricted(adding);
                changed = true;
                break;

            case 'k':
                if (adding)
                    channel->setKey(param);
                else
                    channel->setKey("");
                changed = true;
                break;

            case 'l':
            {
                if (adding)
                {
                    const int limit = std::atoi(param.c_str());
                    if (limit <= 0)
                        continue; // Geçersiz limit: sessizce yok say
                    channel->setUserLimit(limit);
                }
                else
                {
                    channel->setUserLimit(0);
                }
                changed = true;
                break;
            }

            case 'o':
            {
                Client* target = getClientByNick(param);
                if (!target)
                {
                    sendNumeric(client, ERR_NOSUCHNICK, param + " :No such nick/channel");
                    continue;
                }
                if (!channel->isMember(target))
                {
                    sendNumeric(client, ERR_USERNOTINCHANNEL,
                                param + " " + channel->getName() + " :They aren't on that channel");
                    continue;
                }
                if (adding)
                    channel->addOperator(target);
                else
                    channel->removeOperator(target);
                // Yayında istemcinin yazdığı hâli değil, gerçek nick kullanılır
                param = target->getNickname();
                changed = true;
                break;
            }

            default:
                sendNumeric(client, ERR_UNKNOWNMODE, std::string(1, c) + " :is unknown mode char to me");
                continue;
        }

        if (!changed)
            continue;

        const char sign = adding ? '+' : '-';
        if (sign != lastSign)
        {
            appliedModes += sign;
            lastSign = sign;
        }
        appliedModes += c;
        if (!param.empty())
            appliedParams += " " + param;
    }

    if (appliedModes.empty())
        return; // Uygulanan mod yok, yayın da yok

    channel->broadcast(client->getPrefix() + " MODE " + channel->getName() + " "
                       + appliedModes + appliedParams + "\r\n");
}

void Server::handleUserMode(Client* client, Parser& parser)
{
    const std::string target = parser.getParam(0);

    if (!Utils::ircEquals(target, client->getNickname()))
    {
        sendNumeric(client, ERR_USERSDONTMATCH, ":Cannot change mode for other users");
        return;
    }

    // Sorgu
    if (parser.paramCount() < 2)
    {
        sendNumeric(client, RPL_UMODEIS, "+");
        return;
    }

    // Kullanıcı modu tutmuyoruz; istemciyi bekletmemek için mevcut durumu döneriz.
    sendNumeric(client, RPL_UMODEIS, "+");
}
