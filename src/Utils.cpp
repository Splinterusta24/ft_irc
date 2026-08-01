#include "Utils.hpp"

// Bir string'deki tüm harfleri büyük harfe çevirir
std::string Utils::toUpper(const std::string& str)
{
    std::string result = str;
    for (size_t i = 0; i < result.length(); ++i)
    {
        if (result[i] >= 'a' && result[i] <= 'z')
            result[i] = result[i] - 32;
    }
    return result;
}

// Bir string'i verilen ayırıcı karakter ile parçalara ayırır
std::vector<std::string> Utils::split(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter))
    {
        if (!token.empty())
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
