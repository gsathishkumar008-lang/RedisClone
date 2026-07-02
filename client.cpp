#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

int main()
{
    // Initialize WinSock
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cout << "WSAStartup failed\n";
        return 1;
    }

    // Create socket
    SOCKET clientSocket = socket(AF_INET,
                                 SOCK_STREAM,
                                 IPPROTO_TCP);

    if (clientSocket == INVALID_SOCKET)
    {
        cout << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    // Server address
    sockaddr_in serverAddress;

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(6379);
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Connect to server
    if (connect(clientSocket,
                (sockaddr *)&serverAddress,
                sizeof(serverAddress)) == SOCKET_ERROR)
    {
        cout << "Connection failed\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << "Connected to server\n";

    // Send message
    while (true)
    {
        string message;

        cout << "redis> ";
        getline(cin, message);

        if (message == "EXIT")
            break;

        send(clientSocket,
             message.c_str(),
             message.length(),
             0);

        char buffer[1024];

        int bytesReceived = recv(clientSocket,
                                 buffer,
                                 sizeof(buffer),
                                 0);

        if (bytesReceived > 0)
        {
            buffer[bytesReceived] = '\0';
            cout << buffer << endl;
        }
    }

    // Cleanup
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}