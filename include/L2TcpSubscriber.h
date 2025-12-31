// L2TcpSubscriber.h
// TCP订阅客户端 - 连接L2行情服务器，接收实时行情数据
#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <unordered_map>

#include "OrderBook.h"

#ifdef _WIN32
    #include <winsock2.h>
#else
    // Linux/macOS: use sys/socket.h etc. (you can add later)
#endif

/**
 * @class L2TcpSubscriber
 * @brief L2行情TCP订阅客户端
 * 
 * 功能：
 * - 连接到L2行情服务器
 * - 登录认证
 * - 订阅股票行情
 * - 接收和解析行情数据
 * - 分发数据到对应的订单簿
 * 
 * 使用示例：
 * L2TcpSubscriber subscriber(host, port, username, password, "order", &orderbooks);
 * subscriber.connect();
 * subscriber.subscribe("600376.SH");
 */
class L2TcpSubscriber {
public:
    /**
     * @brief 构造函数
     * @param host 服务器地址
     * @param port 服务器端口
     * @param username 登录用户名
     * @param password 登录密码
     * @param type 数据类型："order" 或 "trade"
     * @param orderbooks 订单簿映射表指针
     */
    L2TcpSubscriber(
        const std::string& host,
        int port,
        const std::string& username,
        const std::string& password,
        const std::string& type,
        std::unordered_map<std::string, std::unique_ptr<OrderBook>>* orderbooks
    );

    ~L2TcpSubscriber();

    /**
     * @brief 连接到服务器并启动接收线程
     * @return 连接是否成功
     */
    bool connect();
    
    /**
     * @brief 发送登录请求
     */
    void login();
    
    /**
     * @brief 订阅指定股票
     * @param symbol 股票代码
     */
    void subscribe(const std::string& symbol);
    
    /**
     * @brief 发送数据到服务器
     * @param message 待发送的消息
     */
    void sendData(const std::string& message);
    
    /**
     * @brief 接收服务器数据
     * @return 接收到的数据，失败返回空字符串
     */
    std::string recvData();

    /**
     * @brief 停止接收并断开连接
     */
    void stop();

    /**
     * @brief 检查数据中是否包含指定标志
     * @param data 数据缓冲区
     * @param len 数据长度
     * @param flag 待查找的标志字符串
     * @return 是否包含标志
     */
    bool isContainStrFlag(const char* data, size_t len, const char flag[]);


private:
    void receiveLoop(); // 接收线程主循环

    std::string host_;
    int port_;
    std::string username_;
    std::string password_;
    std::string type_;

    std::atomic<bool> running_{false};
    std::thread recvThread_;
    SOCKET sock_{INVALID_SOCKET};

    std::unordered_map<std::string, std::unique_ptr<OrderBook>>* orderbooks_; // 指向全局订单簿映射
};
