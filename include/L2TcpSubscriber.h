// L2TcpSubscriber.h
#ifndef L2_TCP_SUBSCRIBER_H
#define L2_TCP_SUBSCRIBER_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>

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
        const std::string& password
    );

    ~L2TcpSubscriber();

    bool connect();
    void login();
    void subscribe(const std::string& symbol);
    void sendData(const std::string& message);
    std::string recvData();

    // 停止接收并断开
    void stop();

    // 是否正在运行
    bool isRunning() const { return running_; }

private:
    void receiveLoop(); // 接收线程主循环

    std::string host_;
    int port_;
    std::string username_;
    std::string password_;

    std::atomic<bool> running_{false};
    std::thread recvThread_;
    SOCKET sock_{INVALID_SOCKET};
};

#endif // L2_TCP_SUBSCRIBER_H