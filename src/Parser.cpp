#include "Parser.hpp"
#include "Utils.hpp"

Parser::Parser() {}
Parser::~Parser() {}

// Ham IRC mesajını ayrıştırır
bool Parser::parse(const std::string& line)
{
    clear();
    if (line.empty())
        return false;

    std::string temp = line;
    
    // Mesaj prefix ile başlıyorsa (bizim sunucumuzda client'tan prefix pek gelmez ama gelirse atlıyoruz)
    if (temp[0] == ':')
    {
        size_t spacePos = temp.find(' ');
        if (spacePos == std::string::npos) return false;
        temp = temp.substr(spacePos + 1);
    }

    // Trailing (:) kısmı varsa önce onu ayır
    size_t colonPos = temp.find(" :");
    if (colonPos != std::string::npos)
    {
        _trailing = temp.substr(colonPos + 2);
        temp = temp.substr(0, colonPos);
    }
    // Bazı durumlarda parametre direk : ile başlayabilir (boşluksuz)
    else if (temp.length() > 0 && temp[0] == ':')
    {
        _trailing = temp.substr(1);
        temp = "";
    }

    // Kalan string'i boşluklara göre ayır
    std::vector<std::string> parts = Utils::split(temp, ' ');
    if (parts.empty() && _trailing.empty())
        return false;

    if (!parts.empty())
    {
        // İlk parça komuttur (büyük harfe çeviriyoruz)
        _command = Utils::toUpper(parts[0]);
        // Geri kalanlar parametredir
        for (size_t i = 1; i < parts.size(); ++i)
        {
            _params.push_back(parts[i]);
        }
    }

    return true;
}

std::string Parser::getCommand() const { return _command; }
std::vector<std::string> Parser::getParams() const { return _params; }
std::string Parser::getTrailing() const { return _trailing; }

void Parser::clear()
{
    _command = "";
    _params.clear();
    _trailing = "";
}
