#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client
{
private:
    int         _fd;
    std::string _hostname;
    std::string _nickname;
    std::string _username;
    std::string _realname;
    std::string _inputBuffer;
    std::string _outputBuffer;

    bool        _passwordAccepted;
    bool        _registered;

    // CAP görüşmesi: irssi bağlanır bağlanmaz "CAP LS 302" gönderir ve
    // "CAP END" gelmeden kayıt (registration) tamamlanmamalıdır.
    bool        _capNegotiating;

    // Gecikmeli bağlantı kapatma: komut işlenirken nesne silinemez,
    // önce işaretlenir, poll döngüsünün sonunda temizlenir.
    bool        _markedForQuit;
    std::string _quitReason;

    // Kopyalama engellenir (aynı fd'nin iki kez kapatılmasını önler)
    Client(const Client& other);
    Client& operator=(const Client& other);

public:
    Client(int fd, const std::string& hostname);
    ~Client();

    int getFd() const;
    std::string getHostname() const;

    // ":nick!user@host" biçiminde kaynak öneki. irssi mesajın kimden
    // geldiğini bu maskeye bakarak çözer, sadece ":nick" yetmez.
    std::string getPrefix() const;

    // Gelen veriyi input buffer'a ekler (binary-safe).
    void appendInput(const std::string& data);
    size_t getInputSize() const;
    void clearInput();

    // Buffer'da '\n' ile biten tam bir satır varsa onu 'out'a yazar ve true döner.
    // Tam satır yoksa false döner. Boş satırlar da true ile döner (out boş olur),
    // böylece kalan komutlar aynı turda işlenmeye devam eder.
    bool extractCommand(std::string& out);

    void queueMessage(const std::string& msg);
    bool hasOutput() const;
    std::string getOutputBuffer() const;
    void clearOutput(size_t sentBytes);

    // Soket yazılamaz hâle geldiğinde bekleyen çıktı atılır; aksi hâlde
    // bağlantı hiç kapatılamaz (kapanış çıktının boşalmasını bekler).
    void discardOutput();

    bool isPasswordAccepted() const;
    void setPasswordAccepted(bool status);

    bool isRegistered() const;
    void setRegistered(bool status);

    bool isCapNegotiating() const;
    void setCapNegotiating(bool status);

    bool isMarkedForQuit() const;
    void markForQuit(const std::string& reason);
    std::string getQuitReason() const;

    std::string getNickname() const;
    void setNickname(const std::string& nick);
    std::string getUsername() const;
    void setUsername(const std::string& user);
    std::string getRealname() const;
    void setRealname(const std::string& real);
};

#endif
