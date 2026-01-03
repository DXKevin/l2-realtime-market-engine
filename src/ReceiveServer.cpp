#include "ReceiveServer.h"
#include <iostream>

ReceiveServer::ReceiveServer(const std::string& pipe_name, MessageHandler handler)
    : full_pipe_name_("\\\\.\\pipe\\" + pipe_name), on_message_(handler) {
    server_thread_ = std::thread(&ReceiveServer::runServer, this);

    LOG_INFO("SendServer", "初始化消息服务器" + pipe_name);
}

ReceiveServer::~ReceiveServer() {
    running_ = false;
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void ReceiveServer::runServer() {
    while (running_) {
        HANDLE hPipe = CreateNamedPipeA(
            full_pipe_name_.c_str(),
            PIPE_ACCESS_INBOUND,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1, 1024, 1024, 0, NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }

        // 等待客户端（Node.js）连接
        if (!ConnectNamedPipe(hPipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(hPipe);
            Sleep(500);
            continue;
        }

        // 读取一条消息
        char buffer[1024];
        DWORD bytes_read = 0;
        if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytes_read, NULL) && bytes_read > 0) {
            buffer[bytes_read] = '\0';
            if (on_message_) {
                on_message_(std::string(buffer, bytes_read));
            }
        }

        CloseHandle(hPipe); // 短连接：处理完即关闭
    }
}