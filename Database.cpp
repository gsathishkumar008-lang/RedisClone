#include "Database.h"
#include <fstream>

Database::Database()
{
    loadFromFile();
}

Database::~Database()
{
    saveToFile();
}

void Database::setVal(string key, string value)
{
    checkExpiredKeys();
    db[key] = value;
}

string Database::get(string key)
{
    checkExpiredKeys();
    if (db.find(key) != db.end())
    {
        return db[key];
    }

    return "";
}

bool Database::exists(string key)
{
    checkExpiredKeys();
    return db.find(key) != db.end();
}

bool Database::deleteKey(string key)
{
    checkExpiredKeys();
    auto it = db.find(key);

    if (it == db.end())
        return false;

    db.erase(it);
    expiryMap.erase(key);
    return true;
}

void Database::clearDatabase()
{
    db.clear();
    expiryMap.clear();
}

vector<string> Database::keys()
{
    checkExpiredKeys();
    vector<string> result;

    for (auto &entry : db)
    {
        result.push_back(entry.first);
    }

    return result;
}
void Database::loadFromFile()
{
    ifstream inputFile("database.txt");

    string key, value;

    while (inputFile >> key >> value)
    {
        setVal(key, value);
    }

    inputFile.close();
}

bool Database::expire(string key, int seconds)
{
    checkExpiredKeys();
    if (!exists(key))
    {
        return false;
    }
    time_t expireTime = time(nullptr) + seconds;
    expiryMap[key] = expireTime;
    expiryQueue.push({expireTime, key});
    return true;
}

void Database::checkExpiredKeys()
{
    time_t currentTime = time(nullptr);
    while (!expiryQueue.empty())
    {
        ExpiryNode topNode = expiryQueue.top();
        if (topNode.expiretime > currentTime)
        {
            break;
        }
        expiryQueue.pop();
        if (expiryMap[topNode.key] != topNode.expiretime)
        {
            continue;
        }
        db.erase(topNode.key);
        expiryMap.erase(topNode.key);
    }
}

bool Database::persist(string key){
    checkExpiredKeys();
    if(!exists(key)){
        return false;
    }
    if(expiryMap.find(key) == expiryMap.end()){
        return false;
    }
    expiryMap.erase(key);
    return true;
}

int Database::ttl(string key)
{
    checkExpiredKeys();

    if (!exists(key))
    {
        return -2;
    }

    if (expiryMap.find(key) == expiryMap.end())
    {
        return -1;
    }

    time_t currentTime = time(nullptr);

    return expiryMap[key] - currentTime;
}

void Database::saveToFile()
{
    ofstream outputFile("database.txt");

    if (!outputFile)
    {
        cout << "Failed to create file!" << endl;
        return;
    }

    for (auto &entry : db)  
    {
        outputFile << entry.first << " "
                   << entry.second << endl;
    }

    outputFile.close();
}