#include <bits/stdc++.h>
#include "Database.h"
#include "CommandHandler.h"

using namespace std;

int main()
{
    Database redis;
    string key, value;
    initializeCommands();

    while (true)
    {
        string input;
        getline(cin, input);
        if (input == "EXIT")
            break;

        string response = executeCommand(redis, input);
        cout << response << endl;
    }

    redis.saveToFile();
}