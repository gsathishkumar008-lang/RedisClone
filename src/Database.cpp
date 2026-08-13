#include "Database.h"

void Database::expireKeysUnlocked() {
    const std::time_t now = std::time(nullptr);
    while (!expiryQueue.empty() && expiryQueue.top().expiretime <= now) {
        const ExpiryNode node = expiryQueue.top();
        expiryQueue.pop();
        const auto expiry = expiryMap.find(node.key);
        if (expiry != expiryMap.end() && expiry->second == node.expiretime) {
            db.erase(node.key);
            expiryMap.erase(expiry);
        }
    }
}

void Database::setVal(const std::string& key, const std::string& value) {
    std::unique_lock lock(mutex_);
    expireKeysUnlocked();
    db[key] = value;
    expiryMap.erase(key); // Redis SET removes any existing TTL.
}

bool Database::get(const std::string& key, std::string& out) {
    std::unique_lock lock(mutex_);
    expireKeysUnlocked();
    const auto it = db.find(key);
    if (it == db.end() || !std::holds_alternative<std::string>(it->second)) return false;
    out = std::get<std::string>(it->second);
    return true;
}

bool Database::exists(const std::string& key) {
    std::unique_lock lock(mutex_);
    expireKeysUnlocked();
    return db.find(key) != db.end();
}

bool Database::deleteKey(const std::string& key) {
    std::unique_lock lock(mutex_);
    expireKeysUnlocked();
    const bool deleted = db.erase(key) != 0;
    expiryMap.erase(key);
    return deleted;
}

void Database::clearDatabase() {
    std::unique_lock lock(mutex_);
    db.clear();
    expiryMap.clear();
    expiryQueue = {};
}

std::vector<std::string> Database::keys() {
    std::unique_lock lock(mutex_);
    expireKeysUnlocked();
    std::vector<std::string> result;
    result.reserve(db.size());
    for (const auto& entry : db) result.push_back(entry.first);
    return result;
}

bool Database::expire(const std::string& key, int seconds) {
    std::unique_lock lock(mutex_);
    expireKeysUnlocked();
    if (db.find(key) == db.end()) return false;
    if (seconds <= 0) {
        db.erase(key);
        expiryMap.erase(key);
        return true;
    }
    const std::time_t expiration = std::time(nullptr) + seconds;
    expiryMap[key] = expiration;
    expiryQueue.push({expiration, key});
    return true;
}

void Database::checkExpiredKeys() {
    std::unique_lock lock(mutex_);
    expireKeysUnlocked();
}

bool Database::persist(const std::string& key) {
    std::unique_lock lock(mutex_);
    expireKeysUnlocked();
    if (db.find(key) == db.end()) return false;
    return expiryMap.erase(key) != 0;
}

int Database::ttl(const std::string& key) {
    std::unique_lock lock(mutex_);
    expireKeysUnlocked();
    if (db.find(key) == db.end()) return -2;
    const auto expiry = expiryMap.find(key);
    if (expiry == expiryMap.end()) return -1;
    return static_cast<int>(expiry->second - std::time(nullptr));
}

int Database::lpush(const std::string& key, const std::string& value) {
    std::unique_lock lock(mutex_);
    expireKeysUnlocked();
    auto it = db.find(key);
    if (it == db.end()) {
        db[key] = std::vector<std::string>{value};
        return 1;
    }
    if (!std::holds_alternative<std::vector<std::string>>(it->second)) return -1;
    auto& values = std::get<std::vector<std::string>>(it->second);
    values.insert(values.begin(), value);
    return static_cast<int>(values.size());
}

int Database::lrange(const std::string& key, long long start, long long stop, std::vector<std::string>& out) {
    std::unique_lock lock(mutex_);
    expireKeysUnlocked();
    const auto it = db.find(key);
    if (it == db.end()) {
        out.clear();
        return 0;
    }
    if (!std::holds_alternative<std::vector<std::string>>(it->second)) return -1;

    const auto& values = std::get<std::vector<std::string>>(it->second);
    const long long size = static_cast<long long>(values.size());
    out.clear();
    if (size == 0) return 1;

    long long begin = start;
    long long end = stop;

    if (begin < 0) begin += size;
    if (end < 0) end += size;
    if (begin < 0) begin = 0;
    if (end < 0) end = -1;
    if (begin >= size) {
        out.clear();
        return 1;
    }
    if (end >= size) end = size - 1;
    if (begin > end) {
        out.clear();
        return 1;
    }

    for (long long i = begin; i <= end; ++i) {
        out.push_back(values[static_cast<std::size_t>(i)]);
    }
    return 1;
}

int Database::rpop(const std::string& key, std::string& out) {
    std::unique_lock lock(mutex_);
    expireKeysUnlocked();
    const auto it = db.find(key);
    if (it == db.end()) return 0;
    if (!std::holds_alternative<std::vector<std::string>>(it->second)) return -1;
    auto& values = std::get<std::vector<std::string>>(it->second);
    out = values.back();
    values.pop_back();
    if (values.empty()) db.erase(it);
    return 1;
}
