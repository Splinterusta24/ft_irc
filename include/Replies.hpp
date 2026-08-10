#ifndef REPLIES_HPP
#define REPLIES_HPP

// Sunucu kimliği (tüm numerik cevaplarda prefix olarak kullanılır)
#define SERVER_NAME     "ircserv"
#define SERVER_VERSION  "ft_irc-1.0"

// Desteklenen modlar
#define USER_MODES      "i"
#define CHANNEL_MODES   "itkol"

// Sınırlar
#define MAX_NICK_LEN    30
#define MAX_CHAN_LEN    50
#define MAX_INPUT_SIZE  8192   // Tek satır için üst sınır (flood koruması)
#define RECV_BUFFER     4096

// --- Karşılama / bilgi cevapları ---
#define RPL_WELCOME         "001"
#define RPL_YOURHOST        "002"
#define RPL_CREATED         "003"
#define RPL_MYINFO          "004"
#define RPL_ISUPPORT        "005"

#define RPL_UMODEIS         "221"
#define RPL_CHANNELMODEIS   "324"

#define RPL_NOTOPIC         "331"
#define RPL_TOPIC           "332"
#define RPL_TOPICWHOTIME    "333"
#define RPL_INVITING        "341"

#define RPL_WHOREPLY        "352"
#define RPL_ENDOFWHO        "315"
#define RPL_WHOISUSER       "311"
#define RPL_WHOISSERVER     "312"
#define RPL_WHOISCHANNELS   "319"
#define RPL_ENDOFWHOIS      "318"

#define RPL_NAMREPLY        "353"
#define RPL_ENDOFNAMES      "366"

#define RPL_MOTDSTART       "375"
#define RPL_MOTD            "372"
#define RPL_ENDOFMOTD       "376"

// --- Hata cevapları ---
#define ERR_NOSUCHNICK      "401"
#define ERR_NOSUCHCHANNEL   "403"
#define ERR_CANNOTSENDTOCHAN "404"
#define ERR_NOORIGIN        "409"
#define ERR_NORECIPIENT     "411"
#define ERR_NOTEXTTOSEND    "412"
#define ERR_UNKNOWNCOMMAND  "421"
#define ERR_NONICKNAMEGIVEN "431"
#define ERR_ERRONEUSNICKNAME "432"
#define ERR_NICKNAMEINUSE   "433"
#define ERR_USERNOTINCHANNEL "441"
#define ERR_NOTONCHANNEL    "442"
#define ERR_USERONCHANNEL   "443"
#define ERR_NOTREGISTERED   "451"
#define ERR_NEEDMOREPARAMS  "461"
#define ERR_ALREADYREGISTERED "462"
#define ERR_PASSWDMISMATCH  "464"
#define ERR_CHANNELISFULL   "471"
#define ERR_UNKNOWNMODE     "472"
#define ERR_INVITEONLYCHAN  "473"
#define ERR_BADCHANNELKEY   "475"
#define ERR_CHANOPRIVSNEEDED "482"
#define ERR_UMODEUNKNOWNFLAG "501"
#define ERR_USERSDONTMATCH  "502"

#endif
