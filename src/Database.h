#ifndef DATABASE_H
#define DATABASE_H

#include <ctime>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct ExpiryNode {
    std::time_t expiretime;
    std::string key;
};

struct Compare {
    bool operator()(const ExpiryNode& a, const ExpiryNode& b) const {
        return a.expiretime > b.expiretime;
    }
};

using DatabaseValue = std::variant<std::string, std::vector<std::string>>;

class Database {
private:
    std::unordered_map<std::string, DatabaseValue> db;
    std::unordered_map<std::string, std::time_t> expiryMap;
    std::priority_queue<ExpiryNode, std::vector<ExpiryNode>, Compare> expiryQueue;
    mutable std::shared_mutex mutex_;

    void expireKeysUnlocked();

public:
    Database() = default;
    ~Database() = default;

    void setVal(const std::string& key, const std::string& value);
    bool get(const std::string& key, std::string& out);
    bool exists(const std::string& key);
    bool deleteKey(const std::string& key);
    void clearDatabase();
    std::vector<std::string> keys();

    bool expire(const std::string& key, int seconds);
    void checkExpiredKeys();
    bool persist(const std::string& key);
    int ttl(const std::string& key);

    int lpush(const std::string& key, const std::string& value);
    int lrange(const std::string& key, long long start, long long stop, std::vector<std::string>& out);
    int rpop(const std::string& key, std::string& out);
};

#endif
