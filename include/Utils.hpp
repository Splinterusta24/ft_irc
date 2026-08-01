#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>
#include <sstream>

// Yardımcı fonksiyonlar (String manipulation vb.)
class Utils
{
public:
    static std::string toUpper(const std::string& str);
    static std::vector<std::string> split(const std::string& str, char delimiter);
    static std::string intToString(int number);
};

#endif
