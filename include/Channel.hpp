#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include "Client.hpp"

class Channel
{
private:
    std::string          _name;
    std::string          _topic;
    std::string          _key;
    std::vector<Client*> _members;
    std::vector<Client*> _operators;
    std::vector<Client*> _invited;
    
    // Kanal Modları (Mode)
    bool                 _inviteOnly;
    bool                 _topicRestricted;
    int                  _userLimit;

public:
    Channel(const std::string& name);
    ~Channel();

    std::string getName() const;
    std::string getTopic() const;
    void setTopic(const std::string& topic);

    // Üye işlemleri
    void addMember(Client* client);
    void removeMember(Client* client);
    bool isMember(Client* client) const;
    
    // Operatör işlemleri
    void addOperator(Client* client);
    void removeOperator(Client* client);
    bool isOperator(Client* client) const;

    // Yayınlama
    void broadcast(const std::string& message, Client* exclude = NULL);
    std::string getNamesList() const;
    
    // Mod fonksiyonları
    bool isInviteOnly() const;
    void setInviteOnly(bool status);
    
    bool isTopicRestricted() const;
    void setTopicRestricted(bool status);
    
    std::string getKey() const;
    void setKey(const std::string& key);
    
    int getUserLimit() const;
    void setUserLimit(int limit);
    
    // Davet
    void addInvite(Client* client);
    bool isInvited(Client* client) const;
    void removeInvite(Client* client);
    
    // Üye sayısı
    size_t getMemberCount() const;
};

#endif
