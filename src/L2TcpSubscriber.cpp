// L2TcpSubscriber.cpp
#include <iostream>
#include <sstream>
#include <cassert>

#include "L2TcpSubscriber.h"
#include "logger.h"

#ifdef _WIN32
    #include <ws2tcpip.h>
#else
    // TODO: add POSIX socket support if needed
#endif

static const char* module_name = "L2TcpSubscriber";

// -----------------------
// WinSock 初始化（单例）
// -----------------------
static class WinSockInitializer {
public:
    WinSockInitializer() {
        WSADATA wsaData;
        int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (res != 0) {
            std::cerr << "[WinSock] 初始化失败: " << res << std::endl;
        }
    }
    ~WinSockInitializer() {
        WSACleanup();
    }
} g_winsockInit;

// -----------------------
// L2TcpSubscriber 实现
// -----------------------

L2TcpSubscriber::L2TcpSubscriber(
    const std::string& host,
    int port,
    const std::string& username,
    const std::string& password
) : host_(host), port_(port), username_(username), password_(password)  {
}

L2TcpSubscriber::~L2TcpSubscriber() {
    stop();
}

bool L2TcpSubscriber::connect() {
    LOG_INFO(module_name, "正在连接服务器: {}:{}", host_, port_);

    if (running_) {
        LOG_WARN(module_name, "TCP 已经连接{}:{}", host_, port_);
        return true;
    } 

    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET) {
        LOG_ERROR(module_name, "TCP 创建 socket 失败");
        return false;   
    }

    struct hostent* hptr = gethostbyname(host_.c_str());
    if (!hptr) {
        LOG_ERROR(module_name, "TCP 解析主机失败: {}", host_);
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        return false;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port_));
    addr.sin_addr.S_un.S_addr = inet_addr(inet_ntoa(*(in_addr*)hptr->h_addr_list[0]));

    if (::connect(sock_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        LOG_ERROR(module_name, "TCP 连接失败: {}:{}", host_, port_);
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        return false;
    }

    running_ = true;
    recvThread_ = std::thread(&L2TcpSubscriber::receiveLoop, this);

    login();

    return true;
}

void L2TcpSubscriber::login(){
    std::string loginCmd = "DL," + username_ + "," + password_ + "\n";
    sendData(loginCmd);
}

void L2TcpSubscriber::subscribe(const std::string& symbol) {
    if (!running_ || sock_ == INVALID_SOCKET) {
        LOG_ERROR(module_name, "TCP 未连接，无法订阅 {}:{}", symbol, port_);
        return;
    }

    std::string subCmd = "DY2," + username_ + "," + password_ + "," + symbol;
    sendData(subCmd);
}

void L2TcpSubscriber::sendData(const std::string& message) {
    int sent = send(sock_, message.c_str(), static_cast<int>(message.size()), 0);
    if (sent == SOCKET_ERROR) {
        LOG_ERROR(module_name, "TCP 发送数据失败 <{}:{}>", host_, port_);
    }
}

std::string L2TcpSubscriber::recvData() {
    char buffer[8192] = {0};
    int n = recv(sock_, buffer, sizeof(buffer) - 1, 0);
    if (n == SOCKET_ERROR) {
        LOG_ERROR(module_name, "TCP 接收数据出现错误 <{}:{}>", host_, port_);
        return "";
    }

    if (n == 0) {
        LOG_ERROR(module_name, "TCP 连接已关闭 <{}:{}>", host_, port_);
        return "";
    }

    if (buffer[0] == '<') {
        // 行情流
        // 解析处理
    }

    return std::string(buffer, n);
}

void L2TcpSubscriber::stop() {
    if (!running_) return;

    running_ = false;

    // 关闭 socket 会中断 recv
    if (sock_ != INVALID_SOCKET) {
        shutdown(sock_, SD_BOTH);
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }

    if (recvThread_.joinable()) {
        recvThread_.join();
    }
}

void L2TcpSubscriber::receiveLoop() {
    while (running_) {
        std::string data = recvData();
        if (data.empty()) {
            break; // 连接关闭或出错
        }
        LOG_INFO(module_name, "接收到数据 <{}:{}> : {}", host_, port_, data);
    }
}

