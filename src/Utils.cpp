#include "Utils.hpp"
#include "Replies.hpp"

// Bir string'deki tüm harfleri büyük harfe çevirir
std::string Utils::toUpper(const std::string& str)
{
    std::string result = str;
    for (size_t i = 0; i < result.length(); ++i)
    {
        if (result[i] >= 'a' && result[i] <= 'z')
            result[i] = static_cast<char>(result[i] - 32);
    }
    return result;
}

// IRC casemapping: nick/kanal karşılaştırmaları büyük-küçük harf duyarsızdır.
// rfc1459'a göre [ ] \ ^ karakterleri { } | ~ karakterlerinin büyük hâlidir.
std::string Utils::toIrcLower(const std::string& str)
{
    std::string result = str;
    for (size_t i = 0; i < result.length(); ++i)
    {
        char c = result[i];
        if (c >= 'A' && c <= 'Z')
            result[i] = static_cast<char>(c + 32);
        else if (c == '[')  result[i] = '{';
        else if (c == ']')  result[i] = '}';
        else if (c == '\\') result[i] = '|';
        else if (c == '^')  result[i] = '~';
    }
    return result;
}

bool Utils::ircEquals(const std::string& a, const std::string& b)
{
    return toIrcLower(a) == toIrcLower(b);
}

// Bir string'i verilen ayırıcı karakter ile parçalara ayırır
std::vector<std::string> Utils::split(const std::string& str, char delimiter, bool keepEmpty)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter))
    {
        if (keepEmpty || !token.empty())
            tokens.push_back(token);
    }
    return tokens;
}

// int değerini string'e çevirir (C++98 için std::to_string alternatifi)
std::string Utils::intToString(int number)
{
    std::stringstream ss;
    ss << number;
    return ss.str();
}

// Nick kuralları: ilk karakter harf veya özel karakter olmalı, rakam/'-' ile başlayamaz.
// İzin verilen karakterler: harf, rakam ve "[]\`_^{|}-"
bool Utils::isValidNickname(const std::string& nick)
{
    if (nick.empty() || nick.length() > MAX_NICK_LEN)
        return false;

    const std::string special = "[]\\`_^{|}";

    char first = nick[0];
    bool firstOk = (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z')
                   || special.find(first) != std::string::npos;
    if (!firstOk)
        return false;

    for (size_t i = 1; i < nick.length(); ++i)
    {
        char c = nick[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                  || (c >= '0' && c <= '9') || c == '-'
                  || special.find(c) != std::string::npos;
        if (!ok)
            return false;
    }
    return true;
}

// Kanal adı '#' veya '&' ile başlamalı; boşluk, virgül ve ':' içeremez.
bool Utils::isValidChannelName(const std::string& name)
{
    if (name.length() < 2 || name.length() > MAX_CHAN_LEN)
        return false;
    if (name[0] != '#' && name[0] != '&')
        return false;
    for (size_t i = 1; i < name.length(); ++i)
    {
        char c = name[i];
        if (c == ' ' || c == ',' || c == ':' || c == '\a' || c == '\0')
            return false;
    }
    return true;
}
