#include "Server.hpp"
#include "Replies.hpp"
#include "Utils.hpp"

// ---------------------------------------------------------------------------
// JOIN - Kanala katılma
// Format: JOIN <#kanal1>[,<#kanal2>] [<anahtar1>[,<anahtar2>]]  |  JOIN 0
// ---------------------------------------------------------------------------
void Server::cmdJoin(Client* client, Parser& parser)
{
    std::string target = parser.paramCount() > 0 ? parser.getParam(0) : parser.getTrailing();
    if (target.empty())
    {
        sendNumeric(client, ERR_NEEDMOREPARAMS, "JOIN :Not enough parameters");
        return;
    }

    // "JOIN 0" tüm kanallardan çıkış anlamına gelir
    if (target == "0")
    {
        std::vector<std::string> joined;
        for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        {
            if (it->second->isMember(client))
                joined.push_back(it->second->getName());
        }
        for (size_t i = 0; i < joined.size(); ++i)
            partSingleChannel(client, joined[i], "Leaving all channels");
        return;
    }

    std::vector<std::string> channels = Utils::split(target, ',');
    std::vector<std::string> keys;
    if (parser.paramCount() > 1)
        keys = Utils::split(parser.getParam(1), ',');

    for (size_t i = 0; i < channels.size(); ++i)
    {
        std::string key = (i < keys.size()) ? keys[i] : "";
        joinSingleChannel(client, channels[i], key);
    }
}

void Server::joinSingleChannel(Client* client, const std::string& name, const std::string& key)
{
    if (!Utils::isValidChannelName(name))
    {
        sendNumeric(client, ERR_NOSUCHCHANNEL, name + " :No such channel");
        return;
    }

    Channel* channel = getChannel(name);

    if (!channel)
    {
        // Kanal yoksa oluşturulur ve ilk giren operatör olur
        createChannel(name, client);
        channel = getChannel(name);
    }
    else
    {
        if (channel->isMember(client))
            return; // Zaten üye: sessizce yok say

        if (channel->isInviteOnly() && !channel->isInvited(client))
        {
            sendNumeric(client, ERR_INVITEONLYCHAN, channel->getName() + " :Cannot join channel (+i)");
            return;
        }
        if (channel->hasKey() && channel->getKey() != key)
        {
            sendNumeric(client, ERR_BADCHANNELKEY, channel->getName() + " :Cannot join channel (+k)");
            return;
        }
        if (channel->getUserLimit() > 0
            && channel->getMemberCount() >= static_cast<size_t>(channel->getUserLimit()))
        {
            sendNumeric(client, ERR_CHANNELISFULL, channel->getName() + " :Cannot join channel (+l)");
            return;
        }

        channel->addMember(client);
    }

    // JOIN yankısı kanaldaki herkese (katılan dahil) tam maske ile gönderilir.
    // irssi kanalı ancak kendi JOIN yankısını görünce açar.
    channel->broadcast(client->getPrefix() + " JOIN " + channel->getName() + "\r\n");

    if (!channel->getTopic().empty())
    {
        sendNumeric(client, RPL_TOPIC, channel->getName() + " :" + channel->getTopic());
        sendNumeric(client, RPL_TOPICWHOTIME, channel->getName() + " " + channel->getTopicSetBy() + " 0");
    }

    sendNames(client, channel);
}

// ---------------------------------------------------------------------------
// PART - Kanaldan ayrılma
// ---------------------------------------------------------------------------
void Server::cmdPart(Client* client, Parser& parser)
{
    if (parser.paramCount() < 1)
    {
        sendNumeric(client, ERR_NEEDMOREPARAMS, "PART :Not enough parameters");
        return;
    }

    std::string reason = parser.hasTrailing() ? parser.getTrailing() : "Leaving";
    std::vector<std::string> channels = Utils::split(parser.getParam(0), ',');

    for (size_t i = 0; i < channels.size(); ++i)
        partSingleChannel(client, channels[i], reason);
}

void Server::partSingleChannel(Client* client, const std::string& name, const std::string& reason)
{
    Channel* channel = getChannel(name);
    if (!channel)
    {
        sendNumeric(client, ERR_NOSUCHCHANNEL, name + " :No such channel");
        return;
    }
    if (!channel->isMember(client))
    {
        sendNumeric(client, ERR_NOTONCHANNEL, channel->getName() + " :You're not on that channel");
        return;
    }

    // Ayrılan kişi de mesajı görmeli (irssi pencereyi bu yankı ile kapatır)
    channel->broadcast(client->getPrefix() + " PART " + channel->getName() + " :" + reason + "\r\n");
    channel->removeMember(client);
    removeChannelIfEmpty(channel);
}

// ---------------------------------------------------------------------------
// TOPIC - Konu görüntüleme / değiştirme
// ---------------------------------------------------------------------------
void Server::cmdTopic(Client* client, Parser& parser)
{
    if (parser.paramCount() < 1)
    {
        sendNumeric(client, ERR_NEEDMOREPARAMS, "TOPIC :Not enough parameters");
        return;
    }

    const std::string name = parser.getParam(0);
    Channel* channel = getChannel(name);
    if (!channel)
    {
        sendNumeric(client, ERR_NOSUCHCHANNEL, name + " :No such channel");
        return;
    }
    if (!channel->isMember(client))
    {
        sendNumeric(client, ERR_NOTONCHANNEL, channel->getName() + " :You're not on that channel");
        return;
    }

    // Trailing yoksa bu bir görüntüleme isteğidir
    if (!parser.hasTrailing())
    {
        if (channel->getTopic().empty())
        {
            sendNumeric(client, RPL_NOTOPIC, channel->getName() + " :No topic is set");
        }
        else
        {
            sendNumeric(client, RPL_TOPIC, channel->getName() + " :" + channel->getTopic());
            sendNumeric(client, RPL_TOPICWHOTIME, channel->getName() + " " + channel->getTopicSetBy() + " 0");
        }
        return;
    }

    if (channel->isTopicRestricted() && !channel->isOperator(client))
    {
        sendNumeric(client, ERR_CHANOPRIVSNEEDED, channel->getName() + " :You're not channel operator");
        return;
    }

    channel->setTopic(parser.getTrailing(), client->getNickname());
    channel->broadcast(client->getPrefix() + " TOPIC " + channel->getName() + " :" + channel->getTopic() + "\r\n");
}

// ---------------------------------------------------------------------------
// KICK - Kullanıcıyı kanaldan atma (operatör yetkisi gerekir)
// Format: KICK <#kanal> <nick>[,<nick2>] [:<sebep>]
// ---------------------------------------------------------------------------
void Server::cmdKick(Client* client, Parser& parser)
{
    if (parser.paramCount() < 2)
    {
        sendNumeric(client, ERR_NEEDMOREPARAMS, "KICK :Not enough parameters");
        return;
    }

    const std::string channelName = parser.getParam(0);
    Channel* channel = getChannel(channelName);
    if (!channel)
    {
        sendNumeric(client, ERR_NOSUCHCHANNEL, channelName + " :No such channel");
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

    const std::string reason = parser.hasTrailing() ? parser.getTrailing() : client->getNickname();
    std::vector<std::string> targets = Utils::split(parser.getParam(1), ',');

    for (size_t i = 0; i < targets.size(); ++i)
    {
        Client* target = getClientByNick(targets[i]);
        if (!target)
        {
            sendNumeric(client, ERR_NOSUCHNICK, targets[i] + " :No such nick/channel");
            continue;
        }
        if (!channel->isMember(target))
        {
            sendNumeric(client, ERR_USERNOTINCHANNEL,
                        targets[i] + " " + channel->getName() + " :They aren't on that channel");
            continue;
        }

        // Atılan kişi de KICK mesajını görmeli, bu yüzden önce yayınla sonra çıkar
        channel->broadcast(client->getPrefix() + " KICK " + channel->getName() + " "
                           + target->getNickname() + " :" + reason + "\r\n");
        channel->removeMember(target);
    }

    removeChannelIfEmpty(channel);
}

// ---------------------------------------------------------------------------
// INVITE - Davet gönderme
// Format: INVITE <nick> <#kanal>
// ---------------------------------------------------------------------------
void Server::cmdInvite(Client* client, Parser& parser)
{
    if (parser.paramCount() < 2 && !(parser.paramCount() == 1 && parser.hasTrailing()))
    {
        sendNumeric(client, ERR_NEEDMOREPARAMS, "INVITE :Not enough parameters");
        return;
    }

    const std::string targetNick = parser.getParam(0);
    const std::string channelName = parser.paramCount() > 1 ? parser.getParam(1) : parser.getTrailing();

    Channel* channel = getChannel(channelName);
    if (!channel)
    {
        sendNumeric(client, ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }
    if (!channel->isMember(client))
    {
        sendNumeric(client, ERR_NOTONCHANNEL, channel->getName() + " :You're not on that channel");
        return;
    }
    // Davete kapalı kanallarda sadece operatörler davet edebilir
    if (channel->isInviteOnly() && !channel->isOperator(client))
    {
        sendNumeric(client, ERR_CHANOPRIVSNEEDED, channel->getName() + " :You're not channel operator");
        return;
    }

    Client* target = getClientByNick(targetNick);
    if (!target)
    {
        sendNumeric(client, ERR_NOSUCHNICK, targetNick + " :No such nick/channel");
        return;
    }
    if (channel->isMember(target))
    {
        sendNumeric(client, ERR_USERONCHANNEL,
                    target->getNickname() + " " + channel->getName() + " :is already on channel");
        return;
    }

    channel->addInvite(target);
    sendNumeric(client, RPL_INVITING, target->getNickname() + " " + channel->getName());
    target->queueMessage(client->getPrefix() + " INVITE " + target->getNickname()
                         + " :" + channel->getName() + "\r\n");
}

// ---------------------------------------------------------------------------
// NAMES - Kanal üyelerini listeleme
// ---------------------------------------------------------------------------
void Server::cmdNames(Client* client, Parser& parser)
{
    if (parser.paramCount() < 1)
    {
        sendNumeric(client, RPL_ENDOFNAMES, "* :End of /NAMES list");
        return;
    }

    std::vector<std::string> channels = Utils::split(parser.getParam(0), ',');
    for (size_t i = 0; i < channels.size(); ++i)
    {
        Channel* channel = getChannel(channels[i]);
        if (channel)
            sendNames(client, channel);
        else
            sendNumeric(client, RPL_ENDOFNAMES, channels[i] + " :End of /NAMES list");
    }
}
