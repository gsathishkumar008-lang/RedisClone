#ifndef DATABASE_H
#define DATABASE_H

#include <bits/stdc++.h>
#include <shared_mutex>
#include <variant>
#include <vector>
using namespace std;

struct ExpiryNode{
    time_t expiretime;
    string key;
};
struct Compare{
    bool operator()(ExpiryNode a, ExpiryNode b){
        return a.expiretime>b.expiretime;
    }
};

using DatabaseValue = std::variant<std::string, std::vector<std::string>>;

class Database
{
private:
    unordered_map<string, DatabaseValue> db;
    unordered_map<string,time_t> expiryMap;
    priority_queue<ExpiryNode, vector<ExpiryNode>, Compare> expiryQueue;
    mutable std::shared_mutex mutex_;

    void expireKeysUnlocked();

public:
    Database();
    ~Database();

    void setVal(string key, string value);
    string get(string key);
    bool exists(string key);
    bool deleteKey(string key);
    void clearDatabase();
    vector<string> keys();
    
    bool expire(string key,int seconds);
    void checkExpiredKeys();
    bool persist(string key);
    int ttl(string key);

    void loadFromFile();
    void saveToFile();
    // List operations
    int lpush(string key, string value); // returns new length or -1 on wrong type
    int rpop(string key, string &out);   // returns 1 on success (out set), 0 if missing, -1 on wrong type
};
#endif