#include "AOF.h"
#include <vector>
#include <windows.h>
#include <iostream>

AppendOnlyFile &AppendOnlyFile::instance()
{
    static AppendOnlyFile inst;
    return inst;
}

AppendOnlyFile::AppendOnlyFile()
    : file_(nullptr)
{
}

AppendOnlyFile::~AppendOnlyFile()
{
    close();
}

bool AppendOnlyFile::open(const std::string &path)
{
    std::lock_guard lock(mtx_);
    path_ = path;

    HANDLE h = CreateFileA(
        path_.c_str(),
        FILE_APPEND_DATA | GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (h == INVALID_HANDLE_VALUE)
    {
        std::cerr << "CreateFileA failed: " << GetLastError() << "\n";
        return false;
    }
    file_ = reinterpret_cast<void*>(h);

    // Move file pointer to end to ensure append behavior
    LARGE_INTEGER zero = {};
    if (!SetFilePointerEx(h, zero, nullptr, FILE_END))
    {
        std::cerr << "SetFilePointerEx failed: " << GetLastError() << "\n";
        CloseHandle(h);
        file_ = nullptr;
        return false;
    }

    return true;
}

bool AppendOnlyFile::append(const std::string &data)
{
    std::lock_guard lock(mtx_);
    if (file_ == nullptr)
        return false;

    HANDLE h = reinterpret_cast<HANDLE>(file_);

    // Ensure we're at the end
    LARGE_INTEGER zero = {};
    if (!SetFilePointerEx(h, zero, nullptr, FILE_END))
    {
        std::cerr << "SetFilePointerEx failed: " << GetLastError() << "\n";
        return false;
    }

    DWORD bytesWritten = 0;
    BOOL ok = WriteFile(
        h,
        data.c_str(),
        static_cast<DWORD>(data.size()),
        &bytesWritten,
        nullptr);

    if (!ok || bytesWritten != data.size())
    {
        std::cerr << "WriteFile failed: " << GetLastError() << "\n";
        return false;
    }

    // Write CRLF if not present at end to separate commands
    if (data.size() < 2 || data.substr(data.size() - 2) != "\r\n")
    {
        const char *crlf = "\r\n";
        DWORD w2 = 0;
        WriteFile(h, crlf, 2, &w2, nullptr);
    }

    // Flush by using FlushFileBuffers
    FlushFileBuffers(h);

    return true;
}

static bool readLineFromBuffer(const std::string &buf, size_t &pos, std::string &line)
{
    if (pos >= buf.size())
        return false;
    size_t eol = buf.find("\r\n", pos);
    if (eol == std::string::npos)
        return false;
    line = buf.substr(pos, eol - pos);
    pos = eol + 2;
    return true;
}

bool AppendOnlyFile::readAllCommands(std::vector<std::string> &out)
{
    std::string localPath = path_.empty() ? std::string("data/appendonly.aof") : path_.c_str();

    // Open file for reading
    HANDLE h = CreateFileA(
        localPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (h == INVALID_HANDLE_VALUE)
    {
        return false; // no file
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size))
    {
        CloseHandle(h);
        return false;
    }

    if (size.QuadPart == 0)
    {
        CloseHandle(h);
        return true;
    }

    std::string buf;
    buf.resize(static_cast<size_t>(size.QuadPart));
    DWORD bytesRead = 0;
    if (!ReadFile(h, &buf[0], static_cast<DWORD>(buf.size()), &bytesRead, nullptr))
    {
        CloseHandle(h);
        return false;
    }

    CloseHandle(h);

    size_t pos = 0;
    while (pos < buf.size())
    {
        size_t start = pos;
        std::string line;
        if (!readLineFromBuffer(buf, pos, line))
            break;

        if (line.empty() || line[0] != '*')
            break; // invalid or end

        int numTokens = 0;
        try { numTokens = std::stoi(line.substr(1)); } catch (...) { break; }

        bool ok = true;
        for (int i = 0; i < numTokens; ++i)
        {
            // read $len line
            if (!readLineFromBuffer(buf, pos, line)) { ok = false; break; }
            if (line.empty() || line[0] != '$') { ok = false; break; }
            int len = 0;
            try { len = std::stoi(line.substr(1)); } catch (...) { ok = false; break; }

            // ensure data + CRLF present
            if (pos + static_cast<size_t>(len) + 2 > buf.size()) { ok = false; break; }
            pos += len; // skip data
            // skip CRLF after data
            if (pos + 2 > buf.size()) { ok = false; break; }
            pos += 2;
        }

        if (!ok)
            break;

        size_t end = pos;
        out.push_back(buf.substr(start, end - start));
    }

    return true;
}

void AppendOnlyFile::close()
{
    std::lock_guard lock(mtx_);
    if (file_ != nullptr)
    {
        CloseHandle(reinterpret_cast<HANDLE>(file_));
        file_ = nullptr;
    }
}

bool AppendOnlyFile::isOpen() const
{
    return file_ != nullptr;
}
