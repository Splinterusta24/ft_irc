#include "Client.hpp"

// Constructor: soket (fd) ve bağlantı adresi atanır, durumlar sıfırlanır.
Client::Client(int fd, const std::string& hostname)
    : _fd(fd),
      _hostname(hostname),
      _passwordAccepted(false),
      _registered(false),
      _capNegotiating(false),
      _markedForQuit(false)
{
}

Client::~Client()
{
}

int Client::getFd() const
{
    return _fd;
}

std::string Client::getHostname() const
{
    return _hostname;
}

// Kayıt tamamlanmadan da çağrılabildiği için boş alanlara güvenli varsayılan verilir.
std::string Client::getPrefix() const
{
    std::string nick = _nickname.empty() ? "*" : _nickname;
    std::string user = _username.empty() ? "unknown" : _username;
    std::string host = _hostname.empty() ? "localhost" : _hostname;
    return ":" + nick + "!" + user + "@" + host;
}

// Gelen yeni veriyi mevcut buffer'ın sonuna ekler (kısmi TCP paketleri için gerekli).
void Client::appendInput(const std::string& data)
{
    _inputBuffer += data;
}

size_t Client::getInputSize() const
{
    return _inputBuffer.size();
}

void Client::clearInput()
{
    _inputBuffer.clear();
}

// Buffer'da tam bir satır varsa çıkarır. Dönüş değeri "satır bulundu mu" bilgisidir;
// böylece boş satır (sadece "\r\n") komut akışını kesmez.
bool Client::extractCommand(std::string& out)
{
    size_t pos = _inputBuffer.find('\n');
    if (pos == std::string::npos)
        return false;

    out = _inputBuffer.substr(0, pos);
    _inputBuffer.erase(0, pos + 1);

    // Satır sonundaki '\r' karakterlerini temizler
    while (!out.empty() && (out[out.length() - 1] == '\r' || out[out.length() - 1] == '\n'))
        out.erase(out.length() - 1);

    return true;
}

// Doğrudan send yapmak yerine mesajı output buffer'a ekler (non-blocking I/O).
void Client::queueMessage(const std::string& msg)
{
    if (_markedForQuit && !msg.empty() && msg.compare(0, 6, "ERROR ") != 0)
        return; // Kapanmakta olan bağlantıya yeni mesaj yığmayalım
    _outputBuffer += msg;
}

bool Client::hasOutput() const
{
    return !_outputBuffer.empty();
}

std::string Client::getOutputBuffer() const
{
    return _outputBuffer;
}

// Başarıyla gönderilen miktar (sentBytes) kadar veriyi buffer'dan siler.
void Client::clearOutput(size_t sentBytes)
{
    if (sentBytes <= _outputBuffer.length())
        _outputBuffer.erase(0, sentBytes);
    else
        _outputBuffer.clear();
}

bool Client::isPasswordAccepted() const { return _passwordAccepted; }
void Client::setPasswordAccepted(bool status) { _passwordAccepted = status; }

bool Client::isRegistered() const { return _registered; }
void Client::setRegistered(bool status) { _registered = status; }

bool Client::isCapNegotiating() const { return _capNegotiating; }
void Client::setCapNegotiating(bool status) { _capNegotiating = status; }

bool Client::isMarkedForQuit() const { return _markedForQuit; }

void Client::markForQuit(const std::string& reason)
{
    _markedForQuit = true;
    _quitReason = reason;
}

std::string Client::getQuitReason() const { return _quitReason; }

std::string Client::getNickname() const { return _nickname; }
void Client::setNickname(const std::string& nick) { _nickname = nick; }

std::string Client::getUsername() const { return _username; }
void Client::setUsername(const std::string& user) { _username = user; }

std::string Client::getRealname() const { return _realname; }
void Client::setRealname(const std::string& real) { _realname = real; }
