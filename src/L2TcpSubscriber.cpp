// L2TcpSubscriber.cpp
#include <cassert>
#include <chrono>
#include <iostream>


#include "DataStruct.h"
#include "L2Parser.h"
#include "L2TcpSubscriber.h"
#include "Logger.h"


#ifdef _WIN32
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
// TODO: add POSIX socket support if needed
#endif

static const char *module_name = "L2TcpSubscriber";

inline const std::string &getSymbol(const MarketEvent &evt) {
  if (evt.type == MarketEvent::EventType::ORDER) {
    return std::get<L2Order>(evt.data).symbol;
  } else {
    return std::get<L2Trade>(evt.data).symbol;
  }
}

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
    const std::string &host, int port, const std::string &username,
    const std::string &password, const std::string &type,
    std::unordered_map<std::string, std::unique_ptr<OrderBook>> &orderBooks_ref)
    : host_(host), port_(port), username_(username), password_(password),
      type_(type), orderBooks_ref_(orderBooks_ref), running_(false),
      is_logined_(false), sock_(INVALID_SOCKET) {}

L2TcpSubscriber::~L2TcpSubscriber() { stop(); }

bool L2TcpSubscriber::connect() {
  LOG_INFO(module_name, "正在连接服务器: {}:{}", host_, port_);

  if (!running_) {
    LOG_WARN(module_name, "订阅器已停止，无法连接 {}:{}", host_, port_);
    return false;
  }

  // 如果已经连接并登录，直接返回成功
  if (is_logined_) {
    LOG_INFO(module_name, "已经连接并登录到服务器: {}:{}", host_, port_);
    return true;
  }

  // 确保 socket 是干净的
  if (sock_ != INVALID_SOCKET) {
    LOG_WARN(module_name, "Socket 非法，可能存在残留连接。正在清理...");
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
  }

  // 1. 建立 TCP 连接
  sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock_ == INVALID_SOCKET) {
    LOG_ERROR(module_name, "TCP 创建 socket 失败");
    return false;
  }

  struct addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  struct addrinfo *result = nullptr;
  int res = getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints,
                        &result);
  if (res != 0) {
    LOG_ERROR(module_name, "TCP 解析主机失败: {} (error: {})", host_, res);
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
    return false;
  }

  if (::connect(sock_, result->ai_addr, static_cast<int>(result->ai_addrlen)) ==
      SOCKET_ERROR) {
    LOG_ERROR(module_name, "TCP 连接失败: {}:{}", host_, port_);
    freeaddrinfo(result);
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
    return false;
  }

  freeaddrinfo(result);

  LOG_INFO(module_name, "TCP 连接成功: {}:{}", host_, port_);

  // 2. 发送登录请求
  std::string loginCmd = "<DL," + username_ + "," + password_ + ">";
  int sent =
      send(sock_, loginCmd.c_str(), static_cast<int>(loginCmd.size()), 0);
  if (sent == SOCKET_ERROR) {
    LOG_ERROR(module_name, "TCP 发送登录数据失败 <{}:{}>", host_, port_);
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
    return false;
  }

  // 3. 等待登录响应
  char buffer[8192] = {0};
  int n = recv(sock_, buffer, sizeof(buffer) - 1, 0);

  if (n == SOCKET_ERROR) {
    int err = WSAGetLastError();
    LOG_ERROR(module_name, "TCP 接收登录响应出现错误 <{}:{}>，错误码: {}",
              host_, port_, err);
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
    return false;
  }

  if (n == 0) {
    LOG_ERROR(module_name, "TCP 连接在登录阶段已关闭 <{}:{}>", host_, port_);
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
    return false;
  }

  std::string response(buffer, n);

  if (isContainStrFlag(buffer, n, "Login successful")) {
    LOG_INFO(module_name, "登录成功 <{}:{}>", host_, port_);
    is_logined_.store(true); // 设置合并后的状态

    // 启动接收线程，因为连接和登录都成功了
    running_ = true; // 确保 running_ 为 true
    recvThread_ = std::thread(&L2TcpSubscriber::receiveLoop, this);
    return true;
  } else {
    LOG_ERROR(module_name, "登录失败，服务器返回: {} <{}:{}>", response, host_,
              port_);
    // 登录失败，关闭连接，等待下一次尝试
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
    is_logined_.store(false); // 确保状态为假
    return false;
  }
}

void L2TcpSubscriber::subscribe(const std::string &symbol) {
  if (!is_logined_ || sock_ == INVALID_SOCKET) { // 检查合并后的状态
    LOG_ERROR(module_name, "TCP 未连接或未登录，无法订阅 {}:{}", symbol, port_);
    return;
  }

  std::string subCmd =
      "<DY2," + username_ + "," + password_ + "," + symbol + ">";
  sendData(subCmd);
}

void L2TcpSubscriber::sendData(const std::string &message) {
  if (sock_ == INVALID_SOCKET) {
    LOG_ERROR(module_name, "TCP Socket 无效，无法发送数据");
    return;
  }

  int sent = send(sock_, message.c_str(), static_cast<int>(message.size()), 0);
  if (sent == SOCKET_ERROR) {
    LOG_ERROR(module_name, "TCP 发送数据失败 <{}:{}>", host_, port_);
  }
}

std::string L2TcpSubscriber::recvData() {
  if (sock_ == INVALID_SOCKET) {
    LOG_ERROR(module_name, "TCP Socket 无效，无法接收数据");
    return "";
  }

  char buffer[8192] = {0};
  int n = recv(sock_, buffer, sizeof(buffer) - 1, 0);

  if (n == SOCKET_ERROR) {
    int err = WSAGetLastError();
    LOG_ERROR(module_name, "TCP 接收数据出现错误 <{}:{}>，错误码: {}", host_,
              port_, err);
    return "";
  }

  if (n == 0) {
    LOG_ERROR(module_name, "TCP 连接已关闭 <{}:{}>", host_, port_);
    return "";
  }

  if (isContainStrFlag(buffer, n, "Login successful")) {
    LOG_WARN(module_name, "意外收到登录成功消息（应在连接时处理）");
    return "1";
  }

  if (isContainStrFlag(buffer, n, "DY2,0")) {
    LOG_INFO(module_name, "订阅成功, 返回信息:{}", std::string(buffer, n));
    return "1";
  }

  if (isContainStrFlag(buffer, n, "HeartBeat")) {
    return "1";
  }

  if (isContainStrFlag(buffer, n, "Order") ||
      isContainStrFlag(buffer, n, "Tran")) {
    return "1";
  }

  return std::string(buffer, n);
}

void L2TcpSubscriber::stop() {
  if (!running_)
    return;

  running_ = false;    // 停止所有循环
  is_logined_ = false; // 设置状态为未登录

  // 关闭 socket 会中断 recv
  if (sock_ != INVALID_SOCKET) {
    shutdown(sock_, SD_BOTH);
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
  }

  if (recvThread_.joinable()) {
    recvThread_.join();
  }

  if (connectThread_.joinable()) {
    connectThread_.join();
  }
}

bool L2TcpSubscriber::isContainStrFlag(const char *data, size_t len,
                                       const char flag[]) {
  std::string_view sv(data, len);
  return sv.find(flag) != std::string_view::npos;
}

void L2TcpSubscriber::receiveLoop() {
  LOG_INFO(module_name, "启动接收线程 <{}:{}>", host_, port_);

  while (running_) { // 检查全局运行标志
    std::string data = recvData();

    if (data.empty()) {
      LOG_WARN(module_name, "接收线程检测到连接断开，准备重连 <{}:{}>", host_,
               port_);
      is_logined_ = false; // 设置状态为未登录
      break;               // 退出接收循环，回到 connectLoop 继续尝试
    }

    if (data == "1") {
      continue;
    }

    // 处理行情数据
    auto events = parseL2Data(data, type_, buffer_);
    for (const auto &event : events) {
      const std::string &symbol = getSymbol(event);
      auto it = orderBooks_ref_.find(symbol);
      if (it != orderBooks_ref_.end()) {
        it->second->pushEvent(event);
      } else {
        LOG_WARN(module_name, "未找到对应的 OrderBook 处理数据，合约代码: {}",
                 symbol);
      }
    }
  }
}

void L2TcpSubscriber::connectLoop() {
  while (running_) {
    if (!is_logined_) {
      if (connect()) {
        break;
      } else {
        std::this_thread::sleep_for(std::chrono::seconds(10));
      }
    } else {
      std::this_thread::sleep_for(std::chrono::seconds(10));
    }
  }
}

void L2TcpSubscriber::run() {
  running_ = true;
  connectThread_ = std::thread(&L2TcpSubscriber::connectLoop, this);
}