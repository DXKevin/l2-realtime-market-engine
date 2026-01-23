#pragma once
#include <string>
#include <thread>
#include <atomic>

#include "concurrentqueue/blockingconcurrentqueue.h"

class ReceiveServer {
public:
    ReceiveServer(
        const std::string& pipe_name, 
        moodycamel::BlockingConcurrentQueue<std::string>& monitorEventQueue
    );
    ~ReceiveServer();

    void stop();

private:
    std::string full_pipe_name_;
    std::thread server_thread_;
    std::atomic<bool> running_{true};
    moodycamel::BlockingConcurrentQueue<std::string>& monitorEventQueue_;

    void runServer();
};