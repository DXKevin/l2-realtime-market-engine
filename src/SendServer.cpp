#include "SendServer.h"

SendServer::SendServer(const std::string& pipe_name)
    : full_pipe_name_("\\\\.\\pipe\\" + pipe_name) {
    InitializeCriticalSection(&mutex_);
    server_thread_ = std::thread(&SendServer::runServer, this);
}

SendServer::~SendServer() {
    running_ = false;
    disconnectClient();
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    DeleteCriticalSection(&mutex_);
}

void SendServer::runServer() {
    while (running_) {
        HANDLE hPipe = CreateNamedPipeA(
            full_pipe_name_.c_str(),
            PIPE_ACCESS_OUTBOUND,
            PIPE_TYPE_MESSAGE | PIPE_WAIT,
            1, 1024, 0, 0, NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }

        // 等待客户端（Python）连接
        if (!ConnectNamedPipe(hPipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(hPipe);
            Sleep(500);
            continue;
        }

        // 保存连接句柄
        EnterCriticalSection(&mutex_);
        client_handle_ = hPipe;
        LeaveCriticalSection(&mutex_);
        
        // 保持连接（直到客户端断开或退出）
        while (running_) {
            Sleep(200);
        }

        disconnectClient();
    }
}

void SendServer::disconnectClient() {
    EnterCriticalSection(&mutex_);
    if (client_handle_ != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(client_handle_);
        DisconnectNamedPipe(client_handle_);
        CloseHandle(client_handle_);
        client_handle_ = INVALID_HANDLE_VALUE;
    }
    LeaveCriticalSection(&mutex_);
}

bool SendServer::send(const std::string& message) {
    if (!running_ || message.empty()) return false;

    HANDLE h;
    EnterCriticalSection(&mutex_);
    h = client_handle_;
    LeaveCriticalSection(&mutex_);

    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(h, message.c_str(), (DWORD)message.size(), &written, NULL);
    if (!ok || written != message.size()) {
        disconnectClient();
        return false;
    }
    return true;
}