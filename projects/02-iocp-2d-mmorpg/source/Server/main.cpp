#include "ServerConfig.h"
#include "IocpServer.h"
#include "Logger.h"

#include <iostream>

int main()
{
    IocpServer server;

    if (!server.Start(ServerConfig::SERVER_PORT))
    {
        Logger::Error("Server start failed.");
        return 1;
    }

    Logger::Info("Press ENTER to stop server.");

    std::cin.get();

    server.Stop();

    return 0;
}