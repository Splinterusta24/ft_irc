#include "Server.hpp"
#include "Replies.hpp"
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <string>

// Server.cpp bu değişkeni okur: SIGINT/SIGTERM geldiğinde poll döngüsü kapanır.
volatile sig_atomic_t g_shutdownRequested = 0;

static void handleSignal(int signum)
{
    (void)signum;
    g_shutdownRequested = 1;
}

// Sinyalin poll()'u EINTR ile kesmesi gerekir; bu yüzden SA_RESTART kullanılmaz.
static void installSignalHandlers()
{
    struct sigaction sa;
    sa.sa_handler = &handleSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

// Portu doğrular: yalnızca rakam içermeli ve 1-65535 aralığında olmalı.
static bool parsePort(const std::string& input, int& port)
{
    if (input.empty() || input.length() > 5)
        return false;
    for (size_t i = 0; i < input.length(); ++i)
    {
        if (input[i] < '0' || input[i] > '9')
            return false;
    }
    port = std::atoi(input.c_str());
    return port >= 1 && port <= 65535;
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: ./ircserv <port> <password>" << std::endl;
        return 1;
    }

    int port = 0;
    if (!parsePort(argv[1], port))
    {
        std::cerr << "Error: invalid port. Use a number between 1 and 65535." << std::endl;
        return 1;
    }

    const std::string password = argv[2];
    if (password.empty())
    {
        std::cerr << "Error: password cannot be empty." << std::endl;
        return 1;
    }

    installSignalHandlers();

    try
    {
        Server server(port, password);
        server.init();
        server.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
