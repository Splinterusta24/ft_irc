#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>
#include <sstream>

// Yardımcı fonksiyonlar (String işlemleri, IRC doğrulamaları)
class Utils
{
public:
    static std::string toUpper(const std::string& str);

    // IRC casemapping (rfc1459): A-Z -> a-z ve []\^ -> {}|~
    // Nick ve kanal isimleri bu kurala göre karşılaştırılır.
    static std::string toIrcLower(const std::string& str);
    static bool        ircEquals(const std::string& a, const std::string& b);

    // Ayırıcıya göre böler. keepEmpty=false ise boş parçalar atlanır.
    static std::vector<std::string> split(const std::string& str, char delimiter, bool keepEmpty = false);

    static std::string intToString(int number);

    // IRC doğrulamaları
    static bool isValidNickname(const std::string& nick);
    static bool isValidChannelName(const std::string& name);
};

#endif
