// L2TcpSubscriber.cpp
#include <iostream>
#include <sstream>
#include <cassert>

#include "L2TcpSubscriber.h"
#include "logger.h"
#include "L2Parser.h"

#ifdef _WIN32
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
#else
    // TODO: add POSIX socket support if needed
#endif

static const char* module_name = "L2TcpSubscriber";

// -----------------------
// WinSock 初始化（单例）
// -----------------------
class WinSockInitializer {
public:
    WinSockInitializer() {
        WSADATA wsaData;
        int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (res != 0) {
            std::cerr << "[WinSock] 初始化失败：" << res << std::endl;
            // 但不要退出程序，继续运行
        }
    }

    ~WinSockInitializer() {
        // 只有在初始化成功时才清理
        WSACleanup(); // ← 安全：即使初始化失败，WSACleanup 也安全（Windows 允许）
    }
};

// 全局实例：确保在 main 之前初始化
static WinSockInitializer g_winsockInit;

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

    LOG_INFO(module_name, "TCP 连接成功: {}:{}", host_, port_);

    running_ = true;
    recvThread_ = std::thread(&L2TcpSubscriber::receiveLoop, this);

    login();

    return true;
}

void L2TcpSubscriber::login(){
    if (!running_ || sock_ == INVALID_SOCKET) {
        LOG_ERROR(module_name, "TCP 未连接，无法登录 {}:{}", host_, port_);
        return;
    }

    std::string loginCmd = "DL," + username_ + "," + password_;
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

    if (isContainStrFlag(buffer, n, "Login successful")) {
        LOG_INFO(module_name, "登录成功");
        return "1";
    }

    if (isContainStrFlag(buffer, n, "Subscription successful")) {
        LOG_INFO(module_name, "订阅成功");
        return "1";
    }

    if (isContainStrFlag(buffer, n, "HeartBeat")) {
        return "1";
    }

    return std::string(buffer, n); // 返回行情数据
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

    LOG_INFO(module_name, "已断开与服务器的连接: {}:{}", host_, port_);   
}

bool L2TcpSubscriber::isContainStrFlag(const char* data, size_t len, const char flag[]) {
    std::string_view sv(data, len);
    return sv.find(flag) != std::string_view::npos;
}

void L2TcpSubscriber::receiveLoop() {
    LOG_INFO(module_name, "启动接收线程 <{}:{}>", host_, port_);
    while (running_) {
        std::string data = recvData();

        if (data.empty()) {
            break; // 连接关闭或出错
        }

        if (data == "1") {
            continue; // 登录或订阅成功或心跳包，跳过处理
        }

        LOG_INFO(module_name, "接收到数据 <{}:{}> : {}", host_, port_, data);

        auto orders = parseL2Data<L2Order>(data);

        for (const auto& order : orders) {
            LOG_INFO(module_name,"index:{}, symbol:{}, volume:{}, side:{}",
                order.index,
                order.symbol,
                order.volume,
                order.side
            );
        }
    }
}
