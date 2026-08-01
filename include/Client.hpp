#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client
{
private:
    int         _fd;
    std::string _nickname;
    std::string _username;
    std::string _realname;
    std::string _inputBuffer;
    std::string _outputBuffer;
    bool        _passwordAccepted;
    bool        _registered;

public:
    // Yapıcı fonksiyon (Constructor): Yeni bir Client oluşturulduğunda çağrılır.
    // Soket numarasını (fd) alır ve varsayılan değerleri atar.
    Client(int fd);
    
    // Yıkıcı fonksiyon (Destructor)
    ~Client();

    // fd (dosya tanımlayıcı) değerini döndürür.
    int getFd() const;
    
    // Gelen veriyi (recv ile alınan) input buffer'a ekler.
    void appendInput(const std::string& data);
    
    // Input buffer'dan '\n' veya '\r\n' ile biten tam bir satır (komut) çıkarır.
    // Eğer tam satır yoksa boş döner.
    std::string extractCommand();

    // Gönderilecek veriyi output buffer'a ekler.
    void queueMessage(const std::string& msg);
    
    // Output buffer'ın boş olup olmadığını kontrol eder.
    bool hasOutput() const;
    
    // Output buffer'daki veriyi döndürür.
    std::string getOutputBuffer() const;
    
    // Gönderilen byte kadar veriyi output buffer'dan siler (kısmi send için).
    void clearOutput(size_t sentBytes);

    // Parola onay durumu
    bool isPasswordAccepted() const;
    void setPasswordAccepted(bool status);

    // Kayıt durumu
    bool isRegistered() const;
    void setRegistered(bool status);

    // İsim bilgileri
    std::string getNickname() const;
    void setNickname(const std::string& nick);
    std::string getUsername() const;
    void setUsername(const std::string& user);
    std::string getRealname() const;
    void setRealname(const std::string& real);
};

#endif
