#include "ReceiveServer.h"
#include "Logger.h"

#include <windows.h>


ReceiveServer::ReceiveServer(
    const std::string& pipe_name, 
    moodycamel::BlockingConcurrentQueue<std::string>& monitorEventQueue
): 
full_pipe_name_("\\\\.\\pipe\\" + pipe_name), 
monitorEventQueue_(monitorEventQueue) {

    server_thread_ = std::thread(&ReceiveServer::runServer, this);
    LOG_INFO("ReceiveServer", "初始化监控事件消息服务器" + pipe_name);
}

ReceiveServer::~ReceiveServer() {
    stop();
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
            std::string msg(buffer, bytes_read);

            //检查是否是停止信号
            if (msg == "__POISON_PILL_STOP__") {
                CloseHandle(hPipe);
                break; // 退出循环
            }

            LOG_INFO("ReceiveServer", "收到消息: {}", msg);
            monitorEventQueue_.enqueue(msg);
        }

        CloseHandle(hPipe); // 短连接：处理完即关闭
    }
}

void ReceiveServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    LOG_INFO("ReceiveServer", "停止 ReceiveServer");

    //关键：创建一个临时客户端，发送停止消息
    HANDLE hClient = CreateFileA(
        full_pipe_name_.c_str(),
        GENERIC_WRITE,
        0, NULL,
        OPEN_EXISTING,
        0, NULL
    );

    if (hClient != INVALID_HANDLE_VALUE) {
        const char* stop_msg = "__POISON_PILL_STOP__";
        DWORD written;
        WriteFile(hClient, stop_msg, strlen(stop_msg), &written, NULL);
        CloseHandle(hClient);
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}
