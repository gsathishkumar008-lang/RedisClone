#include "CommandHandler.h"

#include <unordered_map>
#include <functional>
#include <sstream>

using namespace std;

unordered_map<
    string,
    function<string(Database &, vector<string> &)>
> commands;

string handleSet(Database &redis,
                 vector<string> &tokens)
{
    if (tokens.size() != 3)
    {
        return "Usage : SET <key> <value>";
    }

    redis.setVal(tokens[1], tokens[2]);
    return "OK";
}

string handleGet(Database &redis,
                 vector<string> &tokens)
{
    if (tokens.size() != 2)
    {
        return "Usage : GET <key>";
    }

    string value = redis.get(tokens[1]);

    if (value == "")
    {
        return "Key not found";
    }

    return value;
}

string handleDel(Database &redis,
                 vector<string> &tokens)
{
    if (tokens.size() != 2)
    {
        return "Usage : DEL <key>";
    }

    if (redis.deleteKey(tokens[1]))
    {
        return "Deleted successfully";
    }

    return "Key not found";
}

string handleExists(Database &redis,
                    vector<string> &tokens)
{
    if (tokens.size() != 2)
    {
        return "Usage : EXISTS <key>";
    }

    if (redis.exists(tokens[1]))
    {
        return "YES";
    }

    return "NO";
}

string handleKeys(Database &redis,
                  vector<string> &tokens)
{
    if (tokens.size() != 1)
    {
        return "Usage : KEYS";
    }

    vector<string> allKeys = redis.keys();

    string result;

    for (string key : allKeys)
    {
        result += key + "\n";
    }

    return result;
}

string handleClear(Database &redis,
                   vector<string> &tokens)
{
    if (tokens.size() != 1)
    {
        return "Usage : CLEAR";
    }

    redis.clearDatabase();

    return "Database cleared";
}

void initializeCommands()
{
    commands["SET"] = handleSet;
    commands["GET"] = handleGet;
    commands["DEL"] = handleDel;
    commands["EXISTS"] = handleExists;
    commands["KEYS"] = handleKeys;
    commands["CLEAR"] = handleClear;
}

string executeCommand(Database & redis,
                                 vector<string> &tokens)
{
    if (commands.find(tokens[0]) != commands.end())
    {
        return commands[tokens[0]](redis, tokens);
    }
    return "Unknown command";
}