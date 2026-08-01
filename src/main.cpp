#include "../include/Server.hpp"
#include <iostream>
#include <cstdlib>

int main(int argc, char **argv)
{
    // Argüman sayısı kontrolü
    if (argc != 3)
    {
        std::cerr << "Usage: ./ircserv <port> <password>" << std::endl;
        return 1;
    }

    int port = std::atoi(argv[1]);
    std::string password = argv[2];

    // Port değeri doğrulaması
    if (port <= 0 || port > 65535)
    {
        std::cerr << "Error: Invalid port number. Use a port between 1 and 65535." << std::endl;
        return 1;
    }

    try
    {
        // Server nesnesini başlat
        Server server(port, password);
        
        // Soket ve bağlantı ayarlarını yapılandır
        server.init();
        
        // Sonsuz döngüyü başlat (poll event loop)
        server.run();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}
