#include "IOCPServer.h"
#include "Database.h"
#include "CommandHandler.h"

#include <algorithm>
#include <cstring>
#include <iostream>

struct PerIoContext {
    OVERLAPPED overlapped{};
    WSABUF buffer{};
    char data[1024]{};
    SOCKET socket = INVALID_SOCKET;
    enum class IoOperation { Accept, Recv, Send } type = IoOperation::Accept;
    std::string inputBuffer;
    std::string outputBuffer;
    std::size_t sendOffset = 0;
};

IOCPServer::IOCPServer() : listenSocket_(INVALID_SOCKET), iocpHandle_(nullptr), acceptEx_(nullptr), port_(0), isRunning_(false), workerCount_(0) {}
IOCPServer::~IOCPServer() { shutdown(); }

bool IOCPServer::loadAcceptEx() {
    GUID guidAcceptEx = WSAID_ACCEPTEX;
    DWORD bytes = 0;
    return WSAIoctl(listenSocket_, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidAcceptEx, sizeof(guidAcceptEx), &acceptEx_, sizeof(acceptEx_), &bytes, nullptr, nullptr) != SOCKET_ERROR;
}

bool IOCPServer::initialize(unsigned short port) {
    port_ = port;
    listenSocket_ = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (listenSocket_ == INVALID_SOCKET) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR || listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Unable to listen on port " << port_ << ": " << WSAGetLastError() << "\n";
        closesocket(listenSocket_); listenSocket_ = INVALID_SOCKET; return false;
    }
    iocpHandle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (!iocpHandle_ || !CreateIoCompletionPort(reinterpret_cast<HANDLE>(listenSocket_), iocpHandle_, 0, 0) || !loadAcceptEx()) {
        if (iocpHandle_) CloseHandle(iocpHandle_);
        closesocket(listenSocket_); listenSocket_ = INVALID_SOCKET; iocpHandle_ = nullptr; return false;
    }
    return true;
}

bool IOCPServer::postAccept() {
    SOCKET client = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (client == INVALID_SOCKET) return false;
    auto* context = new PerIoContext();
    context->socket = client;
    context->buffer.buf = context->data;
    context->buffer.len = sizeof(context->data);
    context->type = PerIoContext::IoOperation::Accept;
    const BOOL result = acceptEx_(listenSocket_, client, context->data, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, nullptr, &context->overlapped);
    if (!result && WSAGetLastError() != WSA_IO_PENDING) { closesocket(client); delete context; return false; }
    return true;
}

bool IOCPServer::postRecv(PerIoContext* context) {
    ZeroMemory(&context->overlapped, sizeof(context->overlapped));
    context->buffer.buf = context->data;
    context->buffer.len = sizeof(context->data);
    context->type = PerIoContext::IoOperation::Recv;
    DWORD flags = 0;
    const int result = WSARecv(context->socket, &context->buffer, 1, nullptr, &flags, &context->overlapped, nullptr);
    return result != SOCKET_ERROR || WSAGetLastError() == WSA_IO_PENDING;
}

bool IOCPServer::postSend(PerIoContext* context, const std::string& response) {
    if (!response.empty()) { context->outputBuffer = response; context->sendOffset = 0; }
    if (context->sendOffset >= context->outputBuffer.size()) return false;
    ZeroMemory(&context->overlapped, sizeof(context->overlapped));
    context->buffer.buf = context->outputBuffer.data() + context->sendOffset;
    context->buffer.len = static_cast<ULONG>(context->outputBuffer.size() - context->sendOffset);
    context->type = PerIoContext::IoOperation::Send;
    const int result = WSASend(context->socket, &context->buffer, 1, nullptr, 0, &context->overlapped, nullptr);
    return result != SOCKET_ERROR || WSAGetLastError() == WSA_IO_PENDING;
}

void IOCPServer::disconnectClient(PerIoContext* context) {
    if (!context) return;
    if (context->socket != INVALID_SOCKET) closesocket(context->socket);
    delete context;
}

void IOCPServer::onAccept(PerIoContext* context, Database&) {
    if (setsockopt(context->socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<const char*>(&listenSocket_), sizeof(listenSocket_)) == SOCKET_ERROR ||
        !CreateIoCompletionPort(reinterpret_cast<HANDLE>(context->socket), iocpHandle_, 0, 0) || !postRecv(context)) {
        disconnectClient(context);
    }
    if (!postAccept()) std::cerr << "Failed to post next accept: " << WSAGetLastError() << "\n";
}

void IOCPServer::processBufferedCommand(PerIoContext* context, Database& redis) {
    std::string command, parseError;
    const RespParseStatus status = extractRESPCommand(context->inputBuffer, command, parseError);
    if (status == RespParseStatus::Incomplete) { if (!postRecv(context)) disconnectClient(context); return; }
    if (status == RespParseStatus::Error) { context->inputBuffer.clear(); if (!postSend(context, "-ERR " + parseError + "\r\n")) disconnectClient(context); return; }
    if (!postSend(context, executeCommand(redis, command))) disconnectClient(context);
}

void IOCPServer::workerThread(Database& redis) {
    while (isRunning_) {
        DWORD bytes = 0; ULONG_PTR key = 0; OVERLAPPED* overlapped = nullptr;
        const BOOL success = GetQueuedCompletionStatus(iocpHandle_, &bytes, &key, &overlapped, INFINITE);
        if (!overlapped) { if (!isRunning_) break; continue; }
        auto* context = reinterpret_cast<PerIoContext*>(overlapped);
        if (!success) { disconnectClient(context); continue; }
        switch (context->type) {
            case PerIoContext::IoOperation::Accept: onAccept(context, redis); break;
            case PerIoContext::IoOperation::Recv: onRecv(context, bytes, redis); break;
            case PerIoContext::IoOperation::Send: onSend(context, bytes, redis); break;
        }
    }
}

void IOCPServer::onRecv(PerIoContext* context, DWORD bytes, Database& redis) {
    if (bytes == 0) { disconnectClient(context); return; }
    context->inputBuffer.append(context->data, bytes);
    if (context->inputBuffer.size() > 16 * 1024 * 1024) { disconnectClient(context); return; }
    processBufferedCommand(context, redis);
}

void IOCPServer::onSend(PerIoContext* context, DWORD bytes, Database& redis) {
    const std::size_t remaining = context->outputBuffer.size() - context->sendOffset;
    if (bytes == 0 || bytes > remaining) { disconnectClient(context); return; }
    context->sendOffset += bytes;
    if (context->sendOffset < context->outputBuffer.size()) { if (!postSend(context, "")) disconnectClient(context); return; }
    context->outputBuffer.clear();
    processBufferedCommand(context, redis);
}

void IOCPServer::run(Database& redis) {
    workerCount_ = std::max(1u, std::thread::hardware_concurrency());
    isRunning_ = true;
    for (unsigned int i = 0; i < workerCount_; ++i) workerThreads_.emplace_back(&IOCPServer::workerThread, this, std::ref(redis));
    if (!postAccept()) { shutdown(); return; }
    for (auto& thread : workerThreads_) if (thread.joinable()) thread.join();
}

void IOCPServer::shutdown() {
    const bool wasRunning = isRunning_.exchange(false);
    if (iocpHandle_ && wasRunning) for (unsigned int i = 0; i < workerCount_; ++i) PostQueuedCompletionStatus(iocpHandle_, 0, 0, nullptr);
    for (auto& thread : workerThreads_) if (thread.joinable() && thread.get_id() != std::this_thread::get_id()) thread.join();
    workerThreads_.clear();
    if (listenSocket_ != INVALID_SOCKET) { closesocket(listenSocket_); listenSocket_ = INVALID_SOCKET; }
    if (iocpHandle_) { CloseHandle(iocpHandle_); iocpHandle_ = nullptr; }
}
