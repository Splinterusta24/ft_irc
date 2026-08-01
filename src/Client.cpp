#include "Client.hpp"

// Constructor: Client nesnesi oluşturulurken soket (fd) atanır, durumlar sıfırlanır.
Client::Client(int fd) : _fd(fd), _passwordAccepted(false), _registered(false)
{
}

// Destructor
Client::~Client()
{
}

// Client'ın bağlandığı soket numarasını döndürür.
int Client::getFd() const
{
    return _fd;
}

// Gelen yeni veriyi mevcut buffer'ın sonuna ekler. (Kısmi gelen paketler için gerekli)
void Client::appendInput(const std::string& data)
{
    _inputBuffer += data;
}

// Buffer'da bitmiş bir satır (komut) varsa onu bulur, çıkarır ve döndürür.
std::string Client::extractCommand()
{
    std::string cmd = "";
    size_t pos = _inputBuffer.find("\n");
    if (pos != std::string::npos)
    {
        cmd = _inputBuffer.substr(0, pos + 1);
        _inputBuffer.erase(0, pos + 1);
        
        // Satır sonundaki \r ve \n karakterlerini temizler
        while (!cmd.empty() && (cmd[cmd.length() - 1] == '\r' || cmd[cmd.length() - 1] == '\n'))
            cmd.erase(cmd.length() - 1);
    }
    return cmd;
}

// Client'a gönderilecek olan mesajı doğrudan send yapmak yerine output buffer'a ekler.
void Client::queueMessage(const std::string& msg)
{
    _outputBuffer += msg;
}

// Output buffer'da gönderilmeyi bekleyen veri olup olmadığını kontrol eder.
bool Client::hasOutput() const
{
    return !_outputBuffer.empty();
}

// Output buffer'daki tüm veriyi döndürür.
std::string Client::getOutputBuffer() const
{
    return _outputBuffer;
}

// Gönderim yapıldıktan sonra, başarıyla gönderilen miktar (sentBytes) kadar veriyi buffer'dan siler.
void Client::clearOutput(size_t sentBytes)
{
    if (sentBytes <= _outputBuffer.length())
        _outputBuffer.erase(0, sentBytes);
    else
        _outputBuffer.clear();
}

bool Client::isPasswordAccepted() const
{
    return _passwordAccepted;
}

void Client::setPasswordAccepted(bool status)
{
    _passwordAccepted = status;
}

bool Client::isRegistered() const
{
    return _registered;
}

void Client::setRegistered(bool status)
{
    _registered = status;
}

std::string Client::getNickname() const { return _nickname; }
void Client::setNickname(const std::string& nick) { _nickname = nick; }

std::string Client::getUsername() const { return _username; }
void Client::setUsername(const std::string& user) { _username = user; }

std::string Client::getRealname() const { return _realname; }
void Client::setRealname(const std::string& real) { _realname = real; }
