#ifndef DATABASE_H
#define DATABASE_H

#include <bits/stdc++.h>
using namespace std;

class Database
{
private:
    unordered_map<string, string> db;

public:
    Database();
    ~Database();

    void setVal(string key, string value);
    string get(string key);
    bool exists(string key);
    bool deleteKey(string key);
    void clearDatabase();
    vector<string> keys();
    void loadFromFile();
    void saveToFile();
};
#endif