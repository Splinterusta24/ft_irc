#include "Parser.hpp"
#include "Utils.hpp"

Parser::Parser() : _hasTrailing(false) {}
Parser::~Parser() {}

// Ham IRC mesajını RFC 1459 / 2812 kurallarına göre ayrıştırır.
bool Parser::parse(const std::string& line)
{
    clear();
    if (line.empty())
        return false;

    size_t pos = 0;
    const size_t len = line.length();

    // Baştaki boşlukları atla
    while (pos < len && line[pos] == ' ')
        ++pos;

    // Opsiyonel prefix: ":nick!user@host "
    if (pos < len && line[pos] == ':')
    {
        size_t space = line.find(' ', pos);
        if (space == std::string::npos)
            return false;
        _prefix = line.substr(pos + 1, space - pos - 1);
        pos = space;
        while (pos < len && line[pos] == ' ')
            ++pos;
    }

    // Komut
    size_t start = pos;
    while (pos < len && line[pos] != ' ')
        ++pos;
    if (start == pos)
        return false;
    _command = Utils::toUpper(line.substr(start, pos - start));

    // Parametreler
    while (pos < len)
    {
        while (pos < len && line[pos] == ' ')
            ++pos;
        if (pos >= len)
            break;

        // ':' ile başlayan parametre son parametredir ve boşluk içerebilir
        if (line[pos] == ':')
        {
            _trailing = line.substr(pos + 1);
            _hasTrailing = true;
            break;
        }

        start = pos;
        while (pos < len && line[pos] != ' ')
            ++pos;
        _params.push_back(line.substr(start, pos - start));
    }

    return true;
}

std::string Parser::getPrefix() const { return _prefix; }
std::string Parser::getCommand() const { return _command; }
std::vector<std::string> Parser::getParams() const { return _params; }
std::string Parser::getTrailing() const { return _trailing; }
bool Parser::hasTrailing() const { return _hasTrailing; }

size_t Parser::paramCount() const { return _params.size(); }

std::string Parser::getParam(size_t index) const
{
    if (index < _params.size())
        return _params[index];
    return "";
}

size_t Parser::totalParamCount() const
{
    return _params.size() + (_hasTrailing ? 1 : 0);
}

void Parser::clear()
{
    _prefix = "";
    _command = "";
    _params.clear();
    _trailing = "";
    _hasTrailing = false;
}
