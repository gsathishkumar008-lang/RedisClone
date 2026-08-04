#include <iostream>
#include <thread>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Database.h"
#include "CommandHandler.h"
#include "IOCPServer.h"

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

    WSACleanup();

    return 0;
}