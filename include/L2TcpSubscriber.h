// L2TcpSubscriber.h
#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <unordered_map>

#include "OrderBook.h"

#ifdef _WIN32
    #include <winsock2.h>
#else
    // Linux/macOS: use sys/socket.h etc. (you can add later)
#endif

class L2TcpSubscriber {
public:
    
    L2TcpSubscriber(
        const std::string& host,
        int port,
        const std::string& username,
        const std::string& password,
        const std::string& type,
        std::shared_ptr<std::unordered_map<std::string, std::unique_ptr<OrderBook>>> orderbooks_ptr
    );

    ~L2TcpSubscriber();

    bool connect();
    void login();
    void subscribe(const std::string& symbol);
    void sendData(const std::string& message);
    std::string recvData();

    // 停止接收并断开
    void stop();

    bool isContainStrFlag(const char* data, size_t len, const char flag[]);


private:
    void receiveLoop(); // 接收线程主循环

    std::string host_;
    int port_;
    std::string username_;
    std::string password_;
    std::string type_;

    std::string buffer_; // 用于存储接收数据的缓冲区

    std::atomic<bool> running_{false};
    std::thread recvThread_;
    SOCKET sock_{INVALID_SOCKET};

    std::shared_ptr<std::unordered_map<std::string, std::unique_ptr<OrderBook>>> orderbooks_ptr_;
};
