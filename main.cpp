#include <bits/stdc++.h>
#include "Database.h"
using namespace std;

void handleSet(Database &redis,
               vector<string> &tokens)
{
    if (tokens.size() != 3)
    {
        cout << "Usage : SET <key> <value>" << endl;
    }
    else
    {
        redis.setVal(tokens[1], tokens[2]);
        cout << "OK" << endl;
    }
}

void handleGet(Database &redis,
               vector<string> &tokens)
{
    if (tokens.size() != 2)
    {
        cout << "Usage : GET <key>" << endl;
    }
    else
    {
        string value = redis.get(tokens[1]);

        if (value == "")
            cout << "Key not found" << endl;
        else
            cout << value << endl;
    }
}

void handleDel(Database &redis,
               vector<string> &tokens)
{
    if (tokens.size() != 2)
    {
        cout << "Usage : DEL <key>" << endl;
    }
    else
    {
        if (redis.deleteKey(tokens[1]))
        {
            cout << "Deleted successfully" << endl;
        }
        else
        {
            cout << "Key not found" << endl;
        }
    }
}

void handleExists(Database &redis,
                  vector<string> &tokens)
{
    if (tokens.size() != 2)
    {
        cout << "Usage : EXISTS <key>" << endl;
    }
    else
    {
        if (redis.exists(tokens[1]))
        {
            cout << "YES" << endl;
        }
        else
        {
            cout << "NO" << endl;
        }
    }
}

void handleKeys(Database &redis,
                vector<string> &tokens)
{
    if (tokens.size() != 1)
    {
        cout << "Usage : KEYS" << endl;
    }
    else
    {
        vector<string> allKeys = redis.keys();

        for (string key : allKeys)
        {
            cout << key << endl;
        }
    }
}

void handleClear(Database &redis,
                 vector<string> &tokens)
{
    if (tokens.size() != 1)
    {
        cout << "Usage : CLEAR" << endl;
    }
    else
    {
        redis.clearDatabase();
        cout << "Database cleared" << endl;
    }
}

unordered_map<string, function<void(Database &, vector<string> &)>> commands;

void initializeCommands()
{
    commands["SET"] = handleSet;
    commands["GET"] = handleGet;
    commands["DEL"] = handleDel;
    commands["EXISTS"] = handleExists;
    commands["KEYS"] = handleKeys;
    commands["CLEAR"] = handleClear;
}

void executeCommand(Database &redis, vector<string> &tokens) // command dispatcher
{
    if (commands.find(tokens[0]) != commands.end())
    {
        commands[tokens[0]](redis,tokens);
    }
    else
    {
        cout << "Unknown command" << endl;
    }
}

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
        executeCommand(redis, tokens);
    }

    redis.saveToFile();
}