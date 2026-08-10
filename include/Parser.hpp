#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>
#include <vector>

// IRC mesajlarını prefix, komut, parametreler ve trailing kısımlarına ayıran sınıf.
// Format: [":" prefix SPACE] command [params] [SPACE ":" trailing]
class Parser
{
private:
    std::string              _prefix;
    std::string              _command;
    std::vector<std::string> _params;   // Sadece "middle" parametreler
    std::string              _trailing; // ':' ile başlayan son parametre
    bool                     _hasTrailing;

public:
    Parser();
    ~Parser();

    // Gelen ham satırı (örn: "PRIVMSG #test :hello world") parçalar.
    // Komut bulunamazsa false döner.
    bool parse(const std::string& line);

    std::string getPrefix() const;
    std::string getCommand() const;
    std::vector<std::string> getParams() const;
    std::string getTrailing() const;
    bool        hasTrailing() const;

    // Middle parametre sayısı
    size_t paramCount() const;

    // index'teki middle parametreyi döndürür, yoksa boş string.
    std::string getParam(size_t index) const;

    // Trailing dahil toplam parametre sayısı (461 kontrolleri için pratik)
    size_t totalParamCount() const;

    // İşlem sonrası parser'ı temizler
    void clear();
};

#endif
