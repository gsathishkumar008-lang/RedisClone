#include "CommandHandler.h"
#include "AOF.h"

#include <algorithm>
#include <charconv>
#include <functional>
#include <limits>
#include <unordered_map>

namespace {
using Tokens = std::vector<std::string>;
using Handler = std::function<std::string(Database&, const Tokens&)>;
std::unordered_map<std::string, Handler> commands;

std::string simple(const std::string& value) { return "+" + value + "\r\n"; }
std::string integer(long long value) { return ":" + std::to_string(value) + "\r\n"; }
std::string error(const std::string& value) { return "-ERR " + value + "\r\n"; }
std::string bulk(const std::string& value) {
    return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
}
std::string array(const std::vector<std::string>& values) {
    std::string result = "*" + std::to_string(values.size()) + "\r\n";
    for (const auto& value : values) result += bulk(value);
    return result;
}

bool parseNumber(const std::string& line, long long& value) {
    if (line.empty()) return false;
    const auto [ptr, ec] = std::from_chars(line.data(), line.data() + line.size(), value);
    return ec == std::errc{} && ptr == line.data() + line.size();
}

bool parseCommand(const std::string& input, Tokens& tokens, std::string& message) {
    std::size_t pos = 0;
    auto readLine = [&](std::string& line) -> bool {
        const std::size_t end = input.find("\r\n", pos);
        if (end == std::string::npos) return false;
        line.assign(input, pos, end - pos);
        pos = end + 2;
        return true;
    };

    std::string line;
    if (!readLine(line) || line.size() < 2 || line[0] != '*') { message = "Protocol error: expected array"; return false; }
    long long count = 0;
    if (!parseNumber(line.substr(1), count) || count < 0 || count > 1024) { message = "Protocol error: invalid array length"; return false; }
    tokens.clear();
    tokens.reserve(static_cast<std::size_t>(count));
    for (long long i = 0; i < count; ++i) {
        if (!readLine(line) || line.size() < 2 || line[0] != '$') { message = "Protocol error: expected bulk string"; return false; }
        long long length = 0;
        if (!parseNumber(line.substr(1), length) || length < 0 || length > 16 * 1024 * 1024) { message = "Protocol error: invalid bulk length"; return false; }
        const auto size = static_cast<std::size_t>(length);
        if (pos + size + 2 != input.size() && pos + size + 2 > input.size()) { message = "Protocol error: truncated bulk string"; return false; }
        if (pos + size + 2 > input.size() || input.compare(pos + size, 2, "\r\n") != 0) { message = "Protocol error: invalid bulk terminator"; return false; }
        tokens.emplace_back(input, pos, size);
        pos += size + 2;
    }
    if (pos != input.size()) { message = "Protocol error: trailing data"; return false; }
    return true;
}

bool mutates(const std::string& command, const std::string& response) {
    if (command == "SET" || command == "CLEAR" || command == "LPUSH") return response.empty() || response[0] != '-';
    if (command == "DEL" || command == "EXPIRE" || command == "PERSIST" || command == "RPOP") return response.rfind(":1\r\n", 0) == 0 || (command == "RPOP" && response != "$-1\r\n" && response[0] != '-');
    return false;
}
}

void initializeCommands() {
    commands = {
        {"PING", [](Database&, const Tokens& t) { return t.size() == 1 ? simple("PONG") : error("wrong number of arguments for 'ping' command"); }},
        {"SET", [](Database& db, const Tokens& t) { if (t.size() != 3) return error("wrong number of arguments for 'set' command"); db.setVal(t[1], t[2]); return simple("OK"); }},
        {"GET", [](Database& db, const Tokens& t) { if (t.size() != 2) return error("wrong number of arguments for 'get' command"); std::string value; return db.get(t[1], value) ? bulk(value) : "$-1\r\n"; }},
        {"DEL", [](Database& db, const Tokens& t) { if (t.size() != 2) return error("wrong number of arguments for 'del' command"); return integer(db.deleteKey(t[1])); }},
        {"EXISTS", [](Database& db, const Tokens& t) { if (t.size() != 2) return error("wrong number of arguments for 'exists' command"); return integer(db.exists(t[1])); }},
        {"KEYS", [](Database& db, const Tokens& t) { if (t.size() != 1) return error("wrong number of arguments for 'keys' command"); return array(db.keys()); }},
        {"CLEAR", [](Database& db, const Tokens& t) { if (t.size() != 1) return error("wrong number of arguments for 'clear' command"); db.clearDatabase(); return simple("OK"); }},
        {"EXPIRE", [](Database& db, const Tokens& t) { if (t.size() != 3) return error("wrong number of arguments for 'expire' command"); long long seconds; if (!parseNumber(t[2], seconds) || seconds < std::numeric_limits<int>::min() || seconds > std::numeric_limits<int>::max()) return error("value is not an integer or out of range"); return integer(db.expire(t[1], static_cast<int>(seconds))); }},
        {"PERSIST", [](Database& db, const Tokens& t) { if (t.size() != 2) return error("wrong number of arguments for 'persist' command"); return integer(db.persist(t[1])); }},
        {"TTL", [](Database& db, const Tokens& t) { if (t.size() != 2) return error("wrong number of arguments for 'ttl' command"); return integer(db.ttl(t[1])); }},
        {"LPUSH", [](Database& db, const Tokens& t) { if (t.size() != 3) return error("wrong number of arguments for 'lpush' command"); const int result = db.lpush(t[1], t[2]); return result < 0 ? error("WRONGTYPE operation against a key holding the wrong kind of value") : integer(result); }},
        {"RPOP", [](Database& db, const Tokens& t) { if (t.size() != 2) return error("wrong number of arguments for 'rpop' command"); std::string value; const int result = db.rpop(t[1], value); if (result < 0) return error("WRONGTYPE operation against a key holding the wrong kind of value"); return result == 0 ? "$-1\r\n" : bulk(value); }}
    };
}

RespParseStatus extractRESPCommand(std::string& buffer, std::string& command, std::string& message) {
    std::size_t pos = 0;
    auto lineEnd = [&](std::size_t start) { return buffer.find("\r\n", start); };
    const std::size_t headerEnd = lineEnd(pos);
    if (headerEnd == std::string::npos) return RespParseStatus::Incomplete;
    if (headerEnd < 2 || buffer[0] != '*') { message = "Protocol error: expected array"; return RespParseStatus::Error; }
    long long count = 0;
    if (!parseNumber(buffer.substr(1, headerEnd - 1), count) || count < 0 || count > 1024) { message = "Protocol error: invalid array length"; return RespParseStatus::Error; }
    pos = headerEnd + 2;
    for (long long i = 0; i < count; ++i) {
        const std::size_t lengthEnd = lineEnd(pos);
        if (lengthEnd == std::string::npos) return RespParseStatus::Incomplete;
        if (lengthEnd <= pos + 1 || buffer[pos] != '$') { message = "Protocol error: expected bulk string"; return RespParseStatus::Error; }
        long long length = 0;
        if (!parseNumber(buffer.substr(pos + 1, lengthEnd - pos - 1), length) || length < 0 || length > 16 * 1024 * 1024) { message = "Protocol error: invalid bulk length"; return RespParseStatus::Error; }
        pos = lengthEnd + 2;
        const auto size = static_cast<std::size_t>(length);
        if (buffer.size() < pos + size + 2) return RespParseStatus::Incomplete;
        if (buffer.compare(pos + size, 2, "\r\n") != 0) { message = "Protocol error: invalid bulk terminator"; return RespParseStatus::Error; }
        pos += size + 2;
    }
    command.assign(buffer, 0, pos);
    buffer.erase(0, pos);
    return RespParseStatus::Complete;
}

std::string executeCommand(Database& redis, const std::string& input) {
    Tokens tokens;
    std::string message;
    if (!parseCommand(input, tokens, message) || tokens.empty()) return error(message.empty() ? "empty command" : message);
    std::transform(tokens[0].begin(), tokens[0].end(), tokens[0].begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    const auto command = tokens[0];
    const auto it = commands.find(command);
    const std::string response = it == commands.end() ? error("unknown command '" + command + "'") : it->second(redis, tokens);
    if (mutates(command, response) && !AppendOnlyFile::instance().append(input)) {
        return error("persistence failure");
    }
    return response;
}
