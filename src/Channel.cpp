#include "Channel.hpp"
#include "Utils.hpp"

Channel::Channel(const std::string& name)
    : _name(name), _inviteOnly(false), _topicRestricted(false), _userLimit(0)
{
}

Channel::~Channel() {}

std::string Channel::getName() const { return _name; }

std::string Channel::getTopic() const { return _topic; }

void Channel::setTopic(const std::string& topic, const std::string& setBy)
{
    _topic = topic;
    _topicSetBy = setBy;
}

std::string Channel::getTopicSetBy() const { return _topicSetBy; }

void Channel::addMember(Client* client)
{
    if (!isMember(client))
        _members.push_back(client);
    // Kanala girdikten sonra davet kaydı tüketilmiş olur
    removeInvite(client);
}

void Channel::removeMember(Client* client)
{
    for (std::vector<Client*>::iterator it = _members.begin(); it != _members.end(); ++it)
    {
        if (*it == client)
        {
            _members.erase(it);
            break;
        }
    }
    removeOperator(client);
    removeInvite(client);
}

bool Channel::isMember(Client* client) const
{
    for (size_t i = 0; i < _members.size(); ++i)
        if (_members[i] == client) return true;
    return false;
}

const std::vector<Client*>& Channel::getMembers() const { return _members; }

void Channel::addOperator(Client* client)
{
    if (!isOperator(client))
        _operators.push_back(client);
}

void Channel::removeOperator(Client* client)
{
    for (std::vector<Client*>::iterator it = _operators.begin(); it != _operators.end(); ++it)
    {
        if (*it == client)
        {
            _operators.erase(it);
            break;
        }
    }
}

bool Channel::isOperator(Client* client) const
{
    for (size_t i = 0; i < _operators.size(); ++i)
        if (_operators[i] == client) return true;
    return false;
}

void Channel::broadcast(const std::string& message, Client* exclude)
{
    for (size_t i = 0; i < _members.size(); ++i)
    {
        if (_members[i] != exclude)
            _members[i]->queueMessage(message);
    }
}

std::string Channel::getNamesList() const
{
    std::string list;
    for (size_t i = 0; i < _members.size(); ++i)
    {
        if (i > 0)
            list += " ";
        if (isOperator(_members[i]))
            list += "@";
        list += _members[i]->getNickname();
    }
    return list;
}

bool Channel::isInviteOnly() const { return _inviteOnly; }
void Channel::setInviteOnly(bool status) { _inviteOnly = status; }

bool Channel::isTopicRestricted() const { return _topicRestricted; }
void Channel::setTopicRestricted(bool status) { _topicRestricted = status; }

std::string Channel::getKey() const { return _key; }
void Channel::setKey(const std::string& key) { _key = key; }
bool Channel::hasKey() const { return !_key.empty(); }

int Channel::getUserLimit() const { return _userLimit; }
void Channel::setUserLimit(int limit) { _userLimit = (limit < 0) ? 0 : limit; }

std::string Channel::getModeString(bool withParams) const
{
    std::string modes = "+";
    std::string params;

    if (_inviteOnly)
        modes += "i";
    if (_topicRestricted)
        modes += "t";
    if (hasKey())
    {
        modes += "k";
        if (withParams)
            params += " " + _key;
    }
    if (_userLimit > 0)
    {
        modes += "l";
        if (withParams)
            params += " " + Utils::intToString(_userLimit);
    }

    return modes + params;
}

void Channel::addInvite(Client* client)
{
    if (!isInvited(client))
        _invited.push_back(client);
}

bool Channel::isInvited(Client* client) const
{
    for (size_t i = 0; i < _invited.size(); ++i)
        if (_invited[i] == client) return true;
    return false;
}

void Channel::removeInvite(Client* client)
{
    for (std::vector<Client*>::iterator it = _invited.begin(); it != _invited.end(); ++it)
    {
        if (*it == client)
        {
            _invited.erase(it);
            break;
        }
    }
}

size_t Channel::getMemberCount() const { return _members.size(); }

bool Channel::isEmpty() const { return _members.empty(); }
