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
    db[key] = value;
}
string Database::get(string key)
{
    if (db.find(key) != db.end())
    {
        return db[key];
    }

    return "";
}
bool Database::exists(string key)
{
    return db.find(key) != db.end();
}
bool Database::deleteKey(string key)
{
    auto it = db.find(key);

    if (it == db.end())
        return false;

    db.erase(it);
    return true;
}
void Database::clearDatabase()
{
    db.clear();
}
vector<string> Database::keys()
{
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