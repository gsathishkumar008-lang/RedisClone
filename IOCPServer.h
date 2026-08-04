#ifndef IOCPSERVER_H
#define IOCPSERVER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

class Database;
struct PerIoContext;

class IOCPServer
{
public:
    IOCPServer();
    ~IOCPServer();

    bool initialize(unsigned short port);
    void run(Database &redis);
    void shutdown();

private:
    bool loadAcceptEx();
    bool postAccept();
    bool postRecv(struct PerIoContext *context);
    bool postSend(struct PerIoContext *context, const std::string &response);
    void onAccept(struct PerIoContext *context, Database &redis);
    void onRecv(struct PerIoContext *context, DWORD bytesTransferred, Database &redis);
    void onSend(struct PerIoContext *context);
    void disconnectClient(struct PerIoContext *context);
    void workerThread(Database &redis);

    SOCKET listenSocket_;
    HANDLE iocpHandle_;
    LPFN_ACCEPTEX acceptEx_;
    unsigned short port_;
    std::vector<std::thread> workerThreads_;
    std::atomic<bool> isRunning_;
    unsigned int workerCount_;
};

#endif // IOCPSERVER_H
