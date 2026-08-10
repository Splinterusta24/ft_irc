#include "Server.hpp"
#include "Replies.hpp"
#include "Utils.hpp"

// ---------------------------------------------------------------------------
// CAP - Yetenek görüşmesi
// irssi bağlantı açılır açılmaz "CAP LS 302" gönderir ve cevap bekler.
// Cevap verilmezse istemci kayıt akışını tamamlamaz ve zaman aşımına uğrar.
// Hiçbir IRCv3 yeteneği desteklemiyoruz, boş liste dönüp CAP END'i bekliyoruz.
// ---------------------------------------------------------------------------
void Server::cmdCap(Client* client, Parser& parser)
{
    if (parser.paramCount() < 1)
    {
        sendNumeric(client, ERR_NEEDMOREPARAMS, "CAP :Not enough parameters");
        return;
    }

    const std::string sub = Utils::toUpper(parser.getParam(0));
    const std::string nick = client->getNickname().empty() ? "*" : client->getNickname();
    const std::string head = ":" + std::string(SERVER_NAME) + " CAP " + nick + " ";

    if (sub == "LS")
    {
        // Kayıt tamamlanana kadar CAP END beklenecek
        if (!client->isRegistered())
            client->setCapNegotiating(true);
        sendRaw(client, head + "LS :\r\n");
    }
    else if (sub == "LIST")
    {
        sendRaw(client, head + "LIST :\r\n");
    }
    else if (sub == "REQ")
    {
        // Desteklenen yetenek yok: istenen her şey reddedilir
        std::string requested = parser.hasTrailing() ? parser.getTrailing() : parser.getParam(1);
        sendRaw(client, head + "NAK :" + requested + "\r\n");
    }
    else if (sub == "END")
    {
        client->setCapNegotiating(false);
        checkRegistration(client);
    }
    // Bilinmeyen alt komutlar sessizce yok sayılır (410 döndürmek zorunlu değil)
}

// ---------------------------------------------------------------------------
// PASS - Sunucu şifresi
// ---------------------------------------------------------------------------
void Server::cmdPass(Client* client, Parser& parser)
{
    if (client->isRegistered())
    {
        sendNumeric(client, ERR_ALREADYREGISTERED, ":You may not reregister");
        return;
    }

    std::string given = parser.paramCount() > 0 ? parser.getParam(0) : parser.getTrailing();
    if (given.empty())
    {
        sendNumeric(client, ERR_NEEDMOREPARAMS, "PASS :Not enough parameters");
        return;
    }

    if (given != _password)
    {
        sendNumeric(client, ERR_PASSWDMISMATCH, ":Password incorrect");
        sendRaw(client, "ERROR :Closing link: (Bad password)\r\n");
        queueDisconnect(client->getFd(), "Bad password");
        return;
    }

    client->setPasswordAccepted(true);
}

// ---------------------------------------------------------------------------
// NICK - Takma ad belirleme / değiştirme
// ---------------------------------------------------------------------------
void Server::cmdNick(Client* client, Parser& parser)
{
    std::string nick = parser.paramCount() > 0 ? parser.getParam(0) : parser.getTrailing();

    if (nick.empty())
    {
        sendNumeric(client, ERR_NONICKNAMEGIVEN, ":No nickname given");
        return;
    }

    if (!Utils::isValidNickname(nick))
    {
        sendNumeric(client, ERR_ERRONEUSNICKNAME, nick + " :Erroneous nickname");
        return;
    }

    Client* existing = getClientByNick(nick);
    if (existing && existing != client)
    {
        sendNumeric(client, ERR_NICKNAMEINUSE, nick + " :Nickname is already in use");
        return;
    }

    const std::string oldNick = client->getNickname();
    if (oldNick == nick)
        return; // Değişiklik yok

    if (client->isRegistered())
    {
        // Değişikliği aynı kanalları paylaşan herkese ve istemcinin kendisine bildir.
        // Prefix ESKİ nick ile gönderilmelidir, aksi hâlde irssi eşleştiremez.
        const std::string message = client->getPrefix() + " NICK :" + nick + "\r\n";
        broadcastToPeers(client, message, true);
        client->setNickname(nick);
        return;
    }

    client->setNickname(nick);
    checkRegistration(client);
}

// ---------------------------------------------------------------------------
// USER - Kullanıcı bilgisi
// Format: USER <username> <mode> <unused> :<realname>
// ---------------------------------------------------------------------------
void Server::cmdUser(Client* client, Parser& parser)
{
    if (client->isRegistered())
    {
        sendNumeric(client, ERR_ALREADYREGISTERED, ":You may not reregister");
        return;
    }

    if (parser.paramCount() < 3 || !parser.hasTrailing())
    {
        sendNumeric(client, ERR_NEEDMOREPARAMS, "USER :Not enough parameters");
        return;
    }

    client->setUsername(parser.getParam(0));
    client->setRealname(parser.getTrailing());
    checkRegistration(client);
}

// ---------------------------------------------------------------------------
// PING / PONG
// irssi düzenli aralıklarla PING gönderir (lag ölçümü). Cevap gelmezse
// bağlantıyı ölü kabul edip kapatır; bu yüzden PONG zorunludur.
// ---------------------------------------------------------------------------
void Server::cmdPing(Client* client, Parser& parser)
{
    std::string token = parser.paramCount() > 0 ? parser.getParam(0) : parser.getTrailing();
    if (token.empty())
    {
        sendNumeric(client, ERR_NOORIGIN, ":No origin specified");
        return;
    }
    sendRaw(client, ":" + std::string(SERVER_NAME) + " PONG " + SERVER_NAME + " :" + token + "\r\n");
}

void Server::cmdPong(Client* client, Parser& parser)
{
    // İstemcinin bize gönderdiği PONG'a cevap verilmez.
    (void)client;
    (void)parser;
}

// ---------------------------------------------------------------------------
// QUIT
// ---------------------------------------------------------------------------
void Server::cmdQuit(Client* client, Parser& parser)
{
    std::string reason = parser.hasTrailing() ? parser.getTrailing() : parser.getParam(0);
    if (reason.empty())
        reason = "Client Quit";

    sendRaw(client, "ERROR :Closing link: (Quit: " + reason + ")\r\n");
    // Nesne burada silinmez; poll döngüsünün sonunda güvenle temizlenir.
    queueDisconnect(client->getFd(), "Quit: " + reason);
}

// ---------------------------------------------------------------------------
// MOTD
// ---------------------------------------------------------------------------
void Server::cmdMotd(Client* client, Parser& parser)
{
    (void)parser;
    sendMotd(client);
}
