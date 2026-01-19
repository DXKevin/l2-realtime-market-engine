#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <unordered_map>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "OrderBook.h"
#include "AsyncFileWriter.h"

class L2TcpSubscriber {
public:
    L2TcpSubscriber(
        const std::string& host,
        int port,
        const std::string& username,
        const std::string& password,
        const std::string& type,
        std::unordered_map<std::string, std::unique_ptr<OrderBook>>& orderBooks_ref,
        AsyncFileWriter& asyncFileWriter_ref
    );

    ~L2TcpSubscriber();

    // 启动连接/重连循环线程
    void run();

    // 停止所有线程和连接
    void stop();

    // 订阅特定合约
    void subscribe(const std::string& symbol);

    // 尝试连接和登录
    bool login();

    void startLoadHistoryData(const std::string& symbol, const std::string& type);
    void loadHistoryData(const std::string& symbol, const std::string& type);

    std::atomic<bool> is_logined_;
private:


    // 发送数据
    void sendData(const std::string& message);

    // 接收数据
    std::string recvData();

    // 检查数据中是否包含特定标志
    bool isContainStrFlag(const char* data, size_t len, const char flag[]);

    // 接收循环线程函数
    void receiveLoop();

    // 登录循环线程函数
    void loginLoop();

    
    

    // 成员变量
    std::string host_;
    int port_;
    std::string username_;
    std::string password_;
    std::string type_;
    std::unordered_map<std::string, std::unique_ptr<OrderBook>>& orderBooks_ref_;
    AsyncFileWriter& asyncFileWriter_ref_;

    std::atomic<bool> running_;
    SOCKET sock_;
    std::thread recvThread_;
    std::thread loginThread_;
    std::thread historyThread_;

    std::string buffer_; // 用于存储接收数据的缓冲区
};