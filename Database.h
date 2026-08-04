#ifndef DATABASE_H
#define DATABASE_H

#include <bits/stdc++.h>
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

class Database
{
private:
    unordered_map<string, string> db;
    unordered_map<string,time_t> expiryMap;
    priority_queue<ExpiryNode, vector<ExpiryNode>, Compare> expiryQueue;

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
};
#endif