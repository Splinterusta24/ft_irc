#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include "Client.hpp"

class Channel
{
private:
    std::string          _name;      // Görünen ad (kullanıcının yazdığı hâliyle)
    std::string          _topic;
    std::string          _topicSetBy;
    std::string          _key;
    std::vector<Client*> _members;
    std::vector<Client*> _operators;
    std::vector<Client*> _invited;

    // Kanal Modları
    bool                 _inviteOnly;      // +i
    bool                 _topicRestricted; // +t
    int                  _userLimit;       // +l (0 = sınırsız)

    Channel(const Channel& other);
    Channel& operator=(const Channel& other);

public:
    Channel(const std::string& name);
    ~Channel();

    std::string getName() const;

    std::string getTopic() const;
    void setTopic(const std::string& topic, const std::string& setBy);
    std::string getTopicSetBy() const;

    // Üye işlemleri
    void addMember(Client* client);
    void removeMember(Client* client);
    bool isMember(Client* client) const;
    const std::vector<Client*>& getMembers() const;

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
    bool hasKey() const;

    int getUserLimit() const;
    void setUserLimit(int limit);

    // Aktif modları "+itk" biçiminde döndürür.
    // withParams true ise anahtar/limit değerleri de eklenir (sadece üyelere gösterilir).
    std::string getModeString(bool withParams) const;

    // Davet
    void addInvite(Client* client);
    bool isInvited(Client* client) const;
    void removeInvite(Client* client);

    size_t getMemberCount() const;
    bool isEmpty() const;
};

#endif
