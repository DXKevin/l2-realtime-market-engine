#include "SendServer.h"
#include "Logger.h"



SendServer::SendServer(const std::string& pipe_name)
    : full_pipe_name_("\\\\.\\pipe\\" + pipe_name) {
    InitializeCriticalSection(&mutex_);
    server_thread_ = std::thread(&SendServer::runServer, this);

    LOG_INFO("SendServer", "初始化消息服务器" + pipe_name);
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
            Sleep(1000);
            continue;
        }

        LOG_INFO("SendServer", "等待客户端连接到管道: " + full_pipe_name_);
        // 等待客户端（Python）连接
        if (!ConnectNamedPipe(hPipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) {
            LOG_ERROR("SendServer", "ConnectNamedPipe failed.");

            CloseHandle(hPipe);
            Sleep(500);
            continue;
        }

        LOG_INFO("SendServer", "客户端已连接到管道: " + full_pipe_name_);
        // 保存连接句柄
        EnterCriticalSection(&mutex_);
        client_handle_ = hPipe;
        LeaveCriticalSection(&mutex_);

        // 保持连接，同时检测客户端是否断开
        while (running_) {
            // 检测客户端是否断开 - 尝试写入0字节（轻量心跳）
            DWORD bytes_written;

            std::string message = "<HeartBeat>";
            BOOL result = WriteFile(hPipe, message.c_str(), (DWORD)message.size(), &bytes_written, NULL);

            if (!result) {
                DWORD error = GetLastError();
                if (error == ERROR_BROKEN_PIPE ||
                    error == ERROR_NO_DATA ||
                    error == ERROR_PIPE_NOT_CONNECTED) {
                    break; // 客户端已断开
                }
                // 其他错误也视为断开（可选）
                break;
            }
            Sleep(1000);
            LOG_INFO("SendServer", "与客户端保持连接中...");
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