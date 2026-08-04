#include "CommandHandler.h"
#include "AOF.h"

#include <unordered_map>
#include <functional>
#include <sstream>

using namespace std;

unordered_map<
    string,
    function<string(Database &, vector<string> &)>>
    commands;

string handleSet(Database &redis, vector<string> &tokens)
{
    if (tokens.size() != 3)
    {
        return "Usage : SET <key> <value>";
    }

    redis.setVal(tokens[1], tokens[2]);
    return "OK";
}

string handleGet(Database &redis, vector<string> &tokens)
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

string handleDel(Database &redis, vector<string> &tokens)
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

string handleExists(Database &redis, vector<string> &tokens)
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

string handleKeys(Database &redis, vector<string> &tokens)
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

string handleClear(Database &redis,  vector<string> &tokens)
{
    if (tokens.size() != 1)
    {
        return "Usage : CLEAR";
    }

    redis.clearDatabase();

    return "Database cleared";
}

string handleExpire(Database &redis, vector<string> &tokens)
{
    if (tokens.size() != 3)
    {
        return "Usage : EXPIRE <key> <seconds>";
    }

    int seconds = stoi(tokens[2]);

    if (redis.expire(tokens[1], seconds))
    {
        return "1";
    }

    return "0";
}

string handlePersist(Database &redis, vector<string> &tokens)
{
    if (tokens.size() != 2)
    {
        return "Usage : PERSIST <key>";
    }

    if (redis.persist(tokens[1]))
    {
        return "1";
    }

    return "0";
}

string handleTTL(Database &redis,
                 vector<string> &tokens)
{
    if (tokens.size() != 2)
    {
        return "Usage : TTL <key>";
    }

    return to_string(redis.ttl(tokens[1]));
}

string handlePing(Database &redis, vector<string> &tokens)
{
    if (tokens.size() != 1)
    {
        return "-ERR wrong number of arguments for 'PING' command\r\n";
    }

    return "+PONG\r\n";
}

void initializeCommands()
{
    commands["SET"] = handleSet;
    commands["GET"] = handleGet;
    commands["DEL"] = handleDel;
    commands["EXISTS"] = handleExists;
    commands["KEYS"] = handleKeys;
    commands["CLEAR"] = handleClear;
    commands["PING"] = handlePing;
    commands["EXPIRE"] = handleExpire;
    commands["PERSIST"] = handlePersist;
    commands["TTL"] = handleTTL;
    commands["LPUSH"] = handleLPush;
    commands["RPOP"] = handleRPop;

}

/*
Converts a RESP-formatted command into a vector of tokens.

Example:

Input:
*3\r\n
$3\r\nSET\r\n
$4\r\nname\r\n
$7\r\nSathish\r\n

Output:
["SET", "name", "Sathish"]
*/

vector<string> parseRESP(string input)
{
    vector<string> tokens;

    stringstream ss(input);
    string line;

    getline(ss, line);
    if (!line.empty() && line.back() == '\r')
        line.pop_back();

    int numberOfTokens = stoi(line.substr(1));

    for (int i = 0; i < numberOfTokens; i++)
    {
        getline(ss, line); // reads $3, $4, ...
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        getline(ss, line); // reads SET, name, ...
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        tokens.push_back(line);
    }

    return tokens;
}

string executeCommand(Database &redis, string input)
{
    vector<string> tokens = parseRESP(input);
    // tokeninzation
    if (tokens.empty())
    {
        return "";
    }

    cout << "Command = [" << tokens[0] << "]" << endl;
    cout << "Length = " << tokens[0].size() << endl;

    string response;
    if (commands.find(tokens[0]) != commands.end())
    {
        response = commands[tokens[0]](redis, tokens);
    }
    else
    {
        response = "Unknown command";
    }

    // Persist mutating commands to AOF after success
    if (!response.empty())
    {
        if (tokens[0] == "SET" && response == "OK")
        {
            AppendOnlyFile::instance().append(input);
        }
        else if (tokens[0] == "DEL" && response == "Deleted successfully")
        {
            AppendOnlyFile::instance().append(input);
        }
    }

    return response;
}

string handleLPush(Database &redis, vector<string> &tokens)
{
    if (tokens.size() != 3)
    {
        return "Usage : LPUSH <key> <value>";
    }

    int len = redis.lpush(tokens[1], tokens[2]);
    if (len == -1)
        return "-ERR wrong type of value for LPUSH";

    // AOF: append on successful mutation
    // Build RESP for the LPUSH command and append
    string resp = "*3\r\n$5\r\nLPUSH\r\n$" + to_string(tokens[1].size()) + "\r\n" + tokens[1] + "\r\n$" + to_string(tokens[2].size()) + "\r\n" + tokens[2] + "\r\n";
    AppendOnlyFile::instance().append(resp);

    return to_string(len);
}

string handleRPop(Database &redis, vector<string> &tokens)
{
    if (tokens.size() != 2)
    {
        return "Usage : RPOP <key>";
    }

    string out;
    int res = redis.rpop(tokens[1], out);
    if (res == -1)
        return "-ERR wrong type of value for RPOP";
    if (res == 0)
        return "(nil)";

    // AOF: append DEL if list became empty (handled inside rpop) — but we already mutate state so append RPOP for auditing
    string resp = "*2\r\n$4\r\nRPOP\r\n$" + to_string(tokens[1].size()) + "\r\n" + tokens[1] + "\r\n";
    AppendOnlyFile::instance().append(resp);

    return out;
}