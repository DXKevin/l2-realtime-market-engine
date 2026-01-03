#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <windows.h>

#include "Logger.h"

class ReceiveServer {
public:
    using MessageHandler = std::function<void(const std::string&)>;

    ReceiveServer(const std::string& pipe_name, MessageHandler handler);
    ~ReceiveServer();

private:
    std::string full_pipe_name_;
    MessageHandler on_message_;
    std::thread server_thread_;
    std::atomic<bool> running_{true};

    void runServer();
};