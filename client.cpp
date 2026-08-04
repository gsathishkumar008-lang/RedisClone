#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

/*
Converts a plain-text Redis command into RESP format.
string convertToRESP(string input)
Example:

Input:
SET name Sathish

Output:
*3\r\n
$3\r\nSET\r\n
$4\r\nname\r\n
$7\r\nSathish\r\n

The generated RESP string is sent to the Redis server.
*/

string convertToRESP(string input)
{
    stringstream ss(input);

    vector<string> tokens;
    string word;

    while (ss >> word)
    {
        tokens.push_back(word);
    }

    string resp;

    // Convert the number of tokens to a string.
    // Example: 3 -> "3", so the result becomes "*3\r\n"
    resp += "*" + to_string(tokens.size()) + "\r\n";

    for (const string &token : tokens)
    {
        resp += "$" + to_string(token.size()) + "\r\n";
        resp += token + "\r\n";
    }
    return resp;
}

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
        
        string resp = convertToRESP(message);   
        size_t sent = 0;
        while (sent < resp.size())
        {
            int result = send(clientSocket, resp.data() + sent, static_cast<int>(resp.size() - sent), 0);
            if (result == SOCKET_ERROR || result == 0)
            {
                cout << "Send failed\n";
                break;
            }
            sent += static_cast<size_t>(result);
        }
        if (sent != resp.size()) break;

        char buffer[1025];

        int bytesReceived = recv(clientSocket,
                                 buffer,
                                 sizeof(buffer) - 1,
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
