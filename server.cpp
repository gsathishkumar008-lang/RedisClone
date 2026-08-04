#include <iostream>
#include <thread>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Database.h"
#include "CommandHandler.h"
#include "IOCPServer.h"
#include "AOF.h"

using namespace std;

int main()
{
    WSADATA wsaData;
    Database redis;
    initializeCommands();

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cout << "WSAStartup failed\n";
        return 1;
    }

    IOCPServer server;
    if (!server.initialize(6379))
    {
        WSACleanup();
        return 1;
    }

    // Recovery: read and replay commands from appendonly.aof (if present)
    {
        std::vector<std::string> commands;
        if (AppendOnlyFile::instance().readAllCommands(commands))
        {
            for (auto &cmd : commands)
            {
                // executeCommand will not append because AOF is not open yet
                executeCommand(redis, cmd);
            }
        }
    }

    // Open AOF for appending commands
    if (!AppendOnlyFile::instance().open("appendonly.aof"))
    {
        cout << "Warning: could not open appendonly.aof for writing\n";
    }

    std::thread expirationThread([&redis]() {
        while (true)
        {
            redis.checkExpiredKeys();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    expirationThread.detach();

    server.run(redis);
    server.shutdown();

    // Close AOF
    AppendOnlyFile::instance().close();

    WSACleanup();

    return 0;
}