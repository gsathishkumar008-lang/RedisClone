#include <iostream>
#include "Database.h"
#include "CommandHandler.h"
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

int main()
{
    // -------------------------
    // Initialize WinSock
    // -------------------------
    WSADATA wsaData;

    Database redis;
    initializeCommands();

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cout << "WSAStartup failed\n";
        return 1;
    }

    // -------------------------
    // Create Server Socket
    // -------------------------
    SOCKET serverSocket = socket(AF_INET,
                                 SOCK_STREAM,
                                 IPPROTO_TCP);

    if (serverSocket == INVALID_SOCKET)
    {
        cout << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    // -------------------------
    // Server Address
    // -------------------------
    sockaddr_in serverAddress;

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(6379);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // -------------------------
    // Bind
    // -------------------------
    if (bind(serverSocket,
             (sockaddr *)&serverAddress,
             sizeof(serverAddress)) == SOCKET_ERROR)
    {
        cout << "Bind failed\n";

        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // -------------------------
    // Listen
    // -------------------------
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        cout << "Listen failed\n";

        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "Waiting for client...\n";

    // -------------------------
    // Accept Client
    // -------------------------
    while (true)
    {
        SOCKET clientSocket = accept(serverSocket,
                                     nullptr,
                                     nullptr);

        if (clientSocket == INVALID_SOCKET)
        {
            continue;
        }

        cout << "Client Connected!" << endl;

        // recv loop comes here
        while (true)
        {
            char buffer[1024];

            int bytesReceived = recv(clientSocket,
                                     buffer,
                                     sizeof(buffer),
                                     0);

            if (bytesReceived <= 0)
            {
                break;
            }

            buffer[bytesReceived] = '\0';

            string input(buffer);

                  string response = executeCommand(redis,input);

            send(clientSocket,
                 response.c_str(),
                 response.length(),
                 0);
        }

        closesocket(clientSocket);

        cout << "Client Disconnected!" << endl;
    }

    // Cleanup
    // -------------------------
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}