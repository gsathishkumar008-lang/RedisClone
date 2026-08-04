#ifndef AOF_H
#define AOF_H

#include <mutex>
#include <string>
#include <vector>

class AppendOnlyFile
{
public:
    static AppendOnlyFile &instance();

    AppendOnlyFile();
    ~AppendOnlyFile();

    bool open(const std::string &path = "appendonly.aof");
    bool append(const std::string &data);
    void close();
    bool isOpen() const;
    bool readAllCommands(std::vector<std::string> &out);

private:
    void *file_;
    std::mutex mtx_;
    std::string path_;
};

#endif // AOF_H
