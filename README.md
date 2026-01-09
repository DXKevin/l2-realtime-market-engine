<div align="center">

#  L2 实时行情处理引擎

**高性能 Level-2 逐笔行情实时处理系统**

[![C++17](https://img.shields.io/badge/C++-17-blue.svg?style=flat&logo=c%2B%2B)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.29+-064F8C.svg?style=flat&logo=cmake)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows-0078D6.svg?style=flat&logo=windows)](https://www.microsoft.com/windows)
[![License](https://img.shields.io/badge/License-MIT-green.svg?style=flat)](LICENSE)
[![Market](https://img.shields.io/badge/Market-SSE%20%7C%20SZSE-red.svg?style=flat)](https://www.sse.com.cn/)

</div>

---

##  项目简介

**L2 Realtime Market Engine** 是一个基于现代 C++17 标准开发的**高性能、低延迟**的 Level-2 逐笔行情实时处理系统。支持订阅和处理**上海证券交易所（SSE）**和**深圳证券交易所（SZSE）**的实时逐笔委托和成交数据。

###  核心特性

-  **极致性能**：基于无锁并发队列，实现微秒级数据处理延迟
-  **实时订单簿**：事件驱动的订单簿引擎，精确重构市场深度数据
-  **双市场支持**：完整兼容沪深两市的数据协议和格式
-  **异常检测**：内置涨停板撤单监控、封单分析等智能行为识别
-  **灵活集成**：通过命名管道与外部系统（Python/Node.js）无缝对接
-  **历史回放**：支持从 HTTP 下载历史数据进行回测和分析

---

##  系统架构

```

                    系统整体架构                              
──

    TCP 服务器 (L2 行情数据)
           
            逐笔委托端口 (18103)
            逐笔成交端口 (18105)
                    
                    
         
           L2TcpSubscriber     
           - 登录认证          
           - 订阅股票          
           - 接收循环          
         
                    
                    
         
           解析 L2 数据        
           - 字段分割          
           - 数据验证          
           - 创建事件          
         
                    
         
                              
                              
    委托事件              成交事件
                              
         
                    
                    
         
           OrderBook           
           - MPSC 无锁队列     
           - 处理线程          
           - 维护深度          
         
                    
                    
         
           SendServer          
         │  - 命名管道          
           - 推送信号          
         
```

---

##  快速开始

### 环境要求

| 组件 | 版本 | 说明 |
|-----|------|------|
| **操作系统** | Windows 10/11 | 需支持命名管道和 Winsock2 |
| **编译器** | MSVC 2019+ / MinGW-w64 | 需支持 C++17 标准 |
| **CMake** | 3.29+ | 构建系统生成器 |

### 构建步骤

```powershell
# 克隆仓库
git clone https://github.com/yourusername/l2-realtime-market-engine.git
cd l2-realtime-market-engine

# 创建构建目录
mkdir build
cd build

# 生成构建文件
cmake ..

# 编译项目
cmake --build . --config Release
```

### 配置文件

编辑 `bin/config.ini`：

```ini
[server]
host = www.l2api.cn
order_port = 18103
trade_port = 18105
http_url = http://www.l2api.cn

[auth]
username = your_username
password = your_password
```

### 运行程序

```powershell
cd bin
./main.exe
```

---

##  核心模块

### 1. OrderBook（订单簿引擎）

**职责**：维护股票的实时买卖盘深度，处理委托和成交事件

**关键特性**：
- 使用 `std::map` 维护价格档位的有序结构（买盘降序、卖盘升序）
- 使用 `std::list` 实现同价位订单的 FIFO 队列
- 使用 `std::unordered_map` 实现订单 ID 到迭代器的 O(1) 快速查找
- 采用 MPSC 无锁队列接收多生产者事件
- 独立线程处理事件，避免阻塞数据接收

**核心方法**：
```cpp
void pushEvent(const MarketEvent& event);     // 压入事件到队列
void handleOrderEvent(const MarketEvent& e);  // 处理委托事件
void handleTradeEvent(const MarketEvent& e);  // 处理成交事件
void checkLimitUpWithdrawal();                // 检查涨停撤单
```

### 2. L2TcpSubscriber（TCP 订阅客户端）

**职责**：建立与 L2 数据服务器的 TCP 连接，订阅逐笔行情数据流

**关键特性**：
- Winsock2 网络通信
- 独立接收线程持续监听数据
- 自动重连机制
- 数据解析与分发

**核心方法**：
```cpp
bool connect();                               // 连接服务器
void login();                                 // 登录认证
void subscribe(const std::string& symbol);    // 订阅股票
void receiveLoop();                           // 接收循环（后台线程）
```

### 3. L2HttpDownloader（HTTP 下载器）

**职责**：从 HTTP 接口下载历史逐笔数据，用于回测和初始化

**关键特性**：
- 基于 cpp-httplib 的 HTTP 客户端
- 支持 Base64 编码的数据解压
- 异步下载多个数据文件
- 自动解析并推送到 OrderBook

---

##  数据格式说明

### 逐笔委托（Order）数据格式

```
<index,symbol,time,num1,price,volume,type,side,num2,num3,channel,#>
```

| 字段 | 类型 | 说明 | 示例 |
|-----|------|------|------|
| index | int | 推送序号 | 157 |
| symbol | string | 股票代码 | 600895.SH |
| time | string | 时间戳 (HHMMSSmmm) | 93025100 |
| price | int | 价格（单位：0.0001元） | 190000 (19.00元) |
| volume | int | 数量（单位：股） | 100 |
| type | int | 委托类型 | 2 (1=市价, 2=限价, 10=撤单) |
| side | int | 买卖方向 | 1 (1=买入, 2=卖出) |

### 逐笔成交（Trade）数据格式

```
<index,symbol,time,price,volume,money,buy_num,sell_num,type,#>
```

---

##  性能优化

### 优化技术

1. **无锁并发队列**：`moodycamel::BlockingConcurrentQueue` 实现 MPSC 模式
2. **零拷贝解析**：使用 `std::string_view` 避免字符串拷贝
3. **快速整数转换**：`std::from_chars`（比 `std::stoi` 快 3-5 倍）
4. **高效数据结构**：
   - `std::map` 用于价格档位：O(log n)
   - `std::unordered_map` 用于订单查找：O(1)
   - `std::list` 用于同价位订单：O(1) 插入/删除

### 性能指标（参考）

- 单股票事件处理：**> 100,000 events/s**
- 内存占用：**~50MB**（单股票 + 10万订单）
- 延迟：**< 1ms**（事件入队到处理完成）

---

##  项目结构

```
l2-realtime-market-engine/
 main.cpp                    # 程序入口
 CMakeLists.txt              # CMake 构建配置
 LICENSE                     # MIT 许可证
 README.md                   # 本文件

 include/                    # 头文件目录
    OrderBook.h             # 订单簿核心类
    L2TcpSubscriber.h       # TCP 订阅客户端
    L2HttpDownloader.h      # HTTP 下载器
    L2Parser.h              # 数据解析器
    DataStruct.h            # 数据结构定义
    SendServer.h            # 命名管道发送服务
    ReceiveServer.h         # 命名管道接收服务
    Logger.h                # 日志系统接口
    ConfigReader.h          # 配置文件读取

 src/                        # 源文件目录
    OrderBook.cpp
    L2TcpSubscriber.cpp
    L2HttpDownloader.cpp
    SendServer.cpp
    ReceiveServer.cpp
    Logger.cpp

 third_party/                # 第三方库
    include/
        concurrentqueue/    # 无锁并发队列
        httplib/            # HTTP 库
        nlohmann/           # JSON 库
        spdlog/             # 日志库
        zlib/               # 压缩库

 bin/                        # 输出目录
    main.exe                # 可执行文件
    config.ini              # 配置文件
    data/                   # 历史数据
    logs/                   # 日志文件

 build/                      # CMake 构建目录
```

---

##  使用示例

### 订阅单个股票

```cpp
#include "Logger.h"
#include "L2TcpSubscriber.h"
#include "ConfigReader.h"
#include "OrderBook.h"

int main() {
    // 初始化日志系统
    init_log_system("logs/app.log");
    
    // 加载配置
    ConfigReader config("config.ini");
    std::string host = config.get("server", "host");
    int order_port = config.getInt("server", "order_port");
    
    // 创建订单簿
    std::string symbol = "600895.SH";
    auto orderBooksPtr = std::make_shared<std::unordered_map<std::string, std::unique_ptr<OrderBook>>>();
    (*orderBooksPtr)[symbol] = std::make_unique<OrderBook>(symbol, nullptr, nullptr);
    
    // 创建订阅器
    L2TcpSubscriber orderSub(host, order_port, username, password, "order", orderBooksPtr);
    
    // 连接并订阅
    if (orderSub.connect()) {
        orderSub.subscribe(symbol);
    }
    
    std::cin.get();
    return 0;
}
```

### 下载历史数据

```cpp
L2HttpDownloader downloader(http_url, username, password, orderBooksPtr);

// 下载并解析历史数据
downloader.download_and_parse("000592.SZ", "Order");
downloader.download_and_parse("000592.SZ", "Tran");
```

---

##  故障排查

### 常见问题

**Q: 编译错误 - 'std::from_chars' 未找到**  
A: 确保在 CMakeLists.txt 中启用 C++17 标准：
```cmake
set(CMAKE_CXX_STANDARD 17)
```

**Q: WSAStartup 失败，错误代码 10093**  
A: 链接 ws2_32 库：
```cmake
target_link_libraries(common PRIVATE ws2_32)
```

**Q: 命名管道连接失败**  
A: 检查管道名称格式：`\\.\pipe\pipe_name`

---

##  贡献指南

欢迎贡献！请随时提交 Pull Request。

### 步骤

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 开启 Pull Request

---

##  许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

---

##  联系方式

- **问题反馈**：[GitHub Issues](https://github.com/yourusername/l2-realtime-market-engine/issues)
- **邮箱**：your.email@example.com

---

##  致谢

- [moodycamel/concurrentqueue](https://github.com/cameron314/concurrentqueue) - 高性能无锁并发队列
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) - 轻量级 C++ HTTP 库
- [nlohmann/json](https://github.com/nlohmann/json) - 现代 C++ JSON 库
- [gabime/spdlog](https://github.com/gabime/spdlog) - 超快的 C++ 日志库

---

<div align="center">

**专为量化交易打造 **

[ 回到顶部](#-l2-实时行情处理引擎)

</div>
