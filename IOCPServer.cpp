#include "IOCPServer.h"
#include "Database.h"
#include "CommandHandler.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <iostream>

struct PerIoContext
{
    OVERLAPPED overlapped;
    WSABUF buffer;
    char data[1024];
    SOCKET socket;
    bool isReceive;
    enum class IoOperation { Accept, Recv, Send } type;
};

IOCPServer::IOCPServer()
    : listenSocket_(INVALID_SOCKET),
      iocpHandle_(nullptr),
      acceptEx_(nullptr),
      port_(0),
      isRunning_(false),
      workerCount_(0)
{
}

IOCPServer::~IOCPServer()
{
    shutdown();
}

bool IOCPServer::loadAcceptEx()
{
    GUID guidAcceptEx = WSAID_ACCEPTEX;
    DWORD bytes = 0;
    int result = WSAIoctl(
        listenSocket_,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidAcceptEx,
        sizeof(guidAcceptEx),
        &acceptEx_,
        sizeof(acceptEx_),
        &bytes,
        nullptr,
        nullptr);

    return result != SOCKET_ERROR;
}

bool IOCPServer::initialize(unsigned short port)
{
    port_ = port;

    listenSocket_ = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (listenSocket_ == INVALID_SOCKET)
    {
        std::cerr << "WSASocket failed: " << WSAGetLastError() << "\n";
        return false;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        std::cerr << "bind failed: " << WSAGetLastError() << "\n";
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
        return false;
    }

    if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "listen failed: " << WSAGetLastError() << "\n";
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
        return false;
    }

    iocpHandle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (iocpHandle_ == nullptr)
    {
        std::cerr << "CreateIoCompletionPort failed: " << GetLastError() << "\n";
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
        return false;
    }

    if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(listenSocket_), iocpHandle_, reinterpret_cast<ULONG_PTR>(listenSocket_), 0) == nullptr)
    {
        std::cerr << "Associate listen socket failed: " << GetLastError() << "\n";
        closesocket(listenSocket_);
        CloseHandle(iocpHandle_);
        iocpHandle_ = nullptr;
        listenSocket_ = INVALID_SOCKET;
        return false;
    }

    if (!loadAcceptEx())
    {
        std::cerr << "Failed to load AcceptEx: " << WSAGetLastError() << "\n";
        closesocket(listenSocket_);
        CloseHandle(iocpHandle_);
        iocpHandle_ = nullptr;
        listenSocket_ = INVALID_SOCKET;
        return false;
    }

    return true;
}

bool IOCPServer::postAccept()
{
    SOCKET clientSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (clientSocket == INVALID_SOCKET)
    {
        std::cerr << "WSASocket(accept) failed: " << WSAGetLastError() << "\n";
        return false;
    }

    PerIoContext *context = new PerIoContext();
    ZeroMemory(context, sizeof(PerIoContext));
    context->socket = clientSocket;
    context->buffer.buf = context->data;
    context->buffer.len = sizeof(context->data);
    context->isReceive = false;
    context->type = PerIoContext::IoOperation::Accept;

    BOOL result = acceptEx_(
        listenSocket_,
        clientSocket,
        context->data,
        0,
        sizeof(sockaddr_in) + 16,
        sizeof(sockaddr_in) + 16,
        nullptr,
        &context->overlapped);

    if (!result && WSAGetLastError() != WSA_IO_PENDING)
    {
        std::cerr << "AcceptEx failed: " << WSAGetLastError() << "\n";
        closesocket(clientSocket);
        delete context;
        return false;
    }

    return true;
}

bool IOCPServer::postRecv(PerIoContext *context)
{
    ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
    context->buffer.buf = context->data;
    context->buffer.len = sizeof(context->data);
    context->isReceive = true;
    context->type = PerIoContext::IoOperation::Recv;

    DWORD flags = 0;
    DWORD bytesReceived = 0;

    int result = WSARecv(context->socket, &context->buffer, 1, nullptr, &flags, &context->overlapped, nullptr);
    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        std::cerr << "WSARecv failed: " << WSAGetLastError() << "\n";
        return false;
    }

    return true;
}

bool IOCPServer::postSend(PerIoContext *context, const std::string &response)
{
    ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
    memcpy(context->data, response.c_str(), response.size());
    context->buffer.buf = context->data;
    context->buffer.len = static_cast<ULONG>(response.size());
    context->isReceive = false;
    context->type = PerIoContext::IoOperation::Send;

    DWORD bytesSent = 0;
    int result = WSASend(context->socket, &context->buffer, 1, nullptr, 0, &context->overlapped, nullptr);
    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        std::cerr << "WSASend failed: " << WSAGetLastError() << "\n";
        return false;
    }

    return true;
}

void IOCPServer::disconnectClient(PerIoContext *context)
{
    if (context)
    {
        if (context->socket != INVALID_SOCKET)
        {
            closesocket(context->socket);
        }
        delete context;
    }
}

void IOCPServer::onAccept(PerIoContext *context, Database &redis)
{
    if (setsockopt(context->socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<const char*>(&listenSocket_), sizeof(listenSocket_)) == SOCKET_ERROR)
    {
        std::cerr << "SO_UPDATE_ACCEPT_CONTEXT failed: " << WSAGetLastError() << "\n";
        disconnectClient(context);
        return;
    }

    if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(context->socket), iocpHandle_, reinterpret_cast<ULONG_PTR>(context->socket), 0) == nullptr)
    {
        std::cerr << "Associate client socket failed: " << GetLastError() << "\n";
        disconnectClient(context);
        return;
    }

    std::cout << "Client Connected!" << std::endl;
    postRecv(context);
    postAccept();
}

void IOCPServer::workerThread(Database &redis)
{
    while (isRunning_)
    {
        DWORD bytesTransferred;
        ULONG_PTR completionKey;
        OVERLAPPED *pOverlapped = nullptr;

        BOOL success = GetQueuedCompletionStatus(
            iocpHandle_,
            &bytesTransferred,
            &completionKey,
            &pOverlapped,
            INFINITE);

        if (!success && pOverlapped == nullptr)
        {
            if (!isRunning_)
            {
                break;
            }
            continue;
        }

        if (pOverlapped == nullptr)
        {
            continue;
        }

        PerIoContext *context = reinterpret_cast<PerIoContext*>(pOverlapped);
        if (context == nullptr)
        {
            continue;
        }

        if (!success)
        {
            disconnectClient(context);
            continue;
        }

        switch (context->type)
        {
        case PerIoContext::IoOperation::Accept:
            onAccept(context, redis);
            break;
        case PerIoContext::IoOperation::Recv:
            onRecv(context, bytesTransferred, redis);
            break;
        case PerIoContext::IoOperation::Send:
            onSend(context);
            break;
        }
    }
}

void IOCPServer::onRecv(PerIoContext *context, DWORD bytesTransferred, Database &redis)
{
    if (bytesTransferred == 0)
    {
        disconnectClient(context);
        return;
    }

    std::string input(context->data, bytesTransferred);
    std::string response = executeCommand(redis, input);

    if (!postSend(context, response))
    {
        disconnectClient(context);
    }
}

void IOCPServer::onSend(PerIoContext *context)
{
    postRecv(context);
}

void IOCPServer::run(Database &redis)
{
    workerCount_ = std::thread::hardware_concurrency();
    if (workerCount_ == 0)
    {
        workerCount_ = 1;
    }

    isRunning_ = true;
    workerThreads_.reserve(workerCount_);
    for (unsigned int i = 0; i < workerCount_; ++i)
    {
        workerThreads_.emplace_back(&IOCPServer::workerThread, this, std::ref(redis));
    }

    postAccept();

    for (auto &thread : workerThreads_)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
}

void IOCPServer::shutdown()
{
    isRunning_ = false;

    if (iocpHandle_ != nullptr)
    {
        for (unsigned int i = 0; i < workerCount_; ++i)
        {
            PostQueuedCompletionStatus(iocpHandle_, 0, 0, nullptr);
        }
    }

    for (auto &thread : workerThreads_)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    workerThreads_.clear();

    if (listenSocket_ != INVALID_SOCKET)
    {
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
    }

    if (iocpHandle_ != nullptr)
    {
        CloseHandle(iocpHandle_);
        iocpHandle_ = nullptr;
    }
}
