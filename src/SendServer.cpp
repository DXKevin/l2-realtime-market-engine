#include "SendServer.h"
#include "Logger.h"

#include <synchapi.h>
#include <windows.h>


SendServer::SendServer(const std::string& pipe_name)
    : full_pipe_name_("\\\\.\\pipe\\" + pipe_name) {
    InitializeCriticalSection(&mutex_);
    server_thread_ = std::thread(&SendServer::runServer, this);

    LOG_INFO("SendServer", "初始化消息服务器" + pipe_name);
}

SendServer::~SendServer() {
    stop();
}

void SendServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    LOG_INFO("SendServer", "停止 SendServer");

    HANDLE hClient = CreateFileA(
        full_pipe_name_.c_str(),
        GENERIC_READ,          // 客户端只需读（因为服务端是 OUTBOUND）
        0, nullptr,
        OPEN_EXISTING,
        0, nullptr
    );

    if (hClient != INVALID_HANDLE_VALUE) {
        // 连接成功即可，无需读写
        CloseHandle(hClient);
    }

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
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,          // 最多1个实例
            1024,       // 输出缓冲区（服务端 → 客户端）
            1024,       // 输入缓冲区（即使 OUTBOUND，建议非零）
            0,          // 默认超时
            nullptr         // ← 使用默认安全属性
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            LOG_ERROR("SendServer", "CreateNamedPipeA failed with error: " + std::to_string(err));
            Sleep(5000);
            continue;
        }
        
        LOG_INFO("SendServer", "等待客户端连接到管道: " + full_pipe_name_);
        
        // 等待客户端连接
        BOOL connected = ConnectNamedPipe(hPipe, nullptr);

        if (!connected) {
            DWORD err = GetLastError();

            if (err != ERROR_PIPE_CONNECTED) {
                LOG_ERROR("SendServer", "ConnectNamedPipe failed, error: " + std::to_string(err));
                CloseHandle(hPipe);
                Sleep(500);
                continue;
            }
        }

        if (!running_) {
            CloseHandle(hPipe);
            break;
        }

        LOG_INFO("SendServer", "客户端已连接到管道: " + full_pipe_name_);

        // 保存连接句柄
        EnterCriticalSection(&mutex_);
        client_handle_ = hPipe;
        LeaveCriticalSection(&mutex_);

        // 保持连接，同时检测客户端是否断开
        while (running_) {
            bool ok = false;
            EnterCriticalSection(&mutex_);

            if (client_handle_ == hPipe && client_handle_ != INVALID_HANDLE_VALUE) {
                ok = writeUnsafe("<HeartBeat>");
            }

            if (!ok) {
                if (client_handle_ == hPipe){
                    disconnectClientUnsafe();
                }

                LeaveCriticalSection(&mutex_);
                break;
            }

            LeaveCriticalSection(&mutex_);

            LOG_INFO("SendServer", "与客户端保持连接中...");
            Sleep(5000);
        }

        disconnectClient();
    }
}

bool SendServer::send(const std::string& message) {
    if (!running_ || message.empty()) {
        return false;
    }

    EnterCriticalSection(&mutex_);

    if (client_handle_ == INVALID_HANDLE_VALUE) {
        LeaveCriticalSection(&mutex_);
        return false;
    }

    bool ok = writeUnsafe(message);

    if (!ok) {
        disconnectClientUnsafe();
        LeaveCriticalSection(&mutex_);
        return false;
    }

    LeaveCriticalSection(&mutex_);
    return true;
}

bool SendServer::writeUnsafe(const std::string& message) {
    if (client_handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD written = 0;

    BOOL ok = WriteFile(
        client_handle_,
        message.data(),
        static_cast<DWORD>(message.size()),
        &written,
        nullptr
    );

    return ok && written == message.size();
}

void SendServer::disconnectClient() {
    EnterCriticalSection(&mutex_);
    disconnectClientUnsafe();
    LeaveCriticalSection(&mutex_);
}

void SendServer::disconnectClientUnsafe() {
    if (client_handle_ != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(client_handle_);
        DisconnectNamedPipe(client_handle_);
        CloseHandle(client_handle_);
        client_handle_ = INVALID_HANDLE_VALUE;
    }
}