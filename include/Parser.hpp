#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>
#include <vector>

// IRC mesajlarını komut, parametreler ve trailing kısımlarına ayıran sınıf
class Parser
{
private:
    std::string              _command;
    std::vector<std::string> _params;
    std::string              _trailing;

public:
    Parser();
    ~Parser();

    // Gelen ham satırı (örn: "PRIVMSG #test :hello world") parçalar
    bool parse(const std::string& line);

    // Parçalanmış alanlara erişim fonksiyonları
    std::string getCommand() const;
    std::vector<std::string> getParams() const;
    std::string getTrailing() const;
    
    // İşlem sonrası parser'ı temizler
    void clear();
};

#endif
