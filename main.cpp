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

        stringstream ss(input);

        vector<string> tokens;
        string word;

        while (ss >> word)
        {
            tokens.push_back(word);
        }
        string response = executeCommand(redis, tokens);
        cout << response << endl;
    }

    redis.saveToFile();
}