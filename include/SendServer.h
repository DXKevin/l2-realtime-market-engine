#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <windows.h>

#include "logger.h"

class SendServer {
public:
    explicit SendServer(const std::string& pipe_name);
    ~SendServer();

    bool send(const std::string& message); // 主动推送

private:
    std::string full_pipe_name_;
    std::thread server_thread_;
    std::atomic<bool> running_{true};
    HANDLE client_handle_ = INVALID_HANDLE_VALUE;
    CRITICAL_SECTION mutex_;

    void runServer();
    void disconnectClient();
};