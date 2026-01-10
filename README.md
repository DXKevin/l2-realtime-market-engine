# L2 实时行情处理引擎

**高性能 Level-2 逐笔行情实时处理系统**

[![C++17](https://img.shields.io/badge/C++-17-blue.svg?style=flat&logo=c%2B%2B)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.29+-064F8C.svg?style=flat&logo=cmake)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows-0078D6.svg?style=flat&logo=windows)](https://www.microsoft.com/windows)
[![License](https://img.shields.io/badge/License-MIT-green.svg?style=flat)](LICENSE)

---

## 项目简介

**L2 Realtime Market Engine** 是一个基于现代 C++17 标准开发的**高性能、低延迟** Level-2 逐笔行情实时处理系统。支持订阅和处理**上海证券交易所（SSE）**和**深圳证券交易所（SZSE）**的实时逐笔委托和成交数据。

### 核心特性

- 🚀 **极致性能**：基于无锁并发队列（`moodycamel::BlockingConcurrentQueue`），实现微秒级数据处理延迟
- 📊 **实时订单簿**：事件驱动的订单簿引擎，精确重构市场深度数据
- 🔄 **双市场支持**：完整兼容沪深两市的数据协议和格式差异
- 🔍 **异常检测**：内置涨停板撤单监控、封单分析等智能行为识别
- 🔌 **灵活集成**：通过命名管道与外部系统（Python/Node.js）无缝对接
- 📈 **历史回放**：支持从 HTTP 下载历史数据进行回测和分析

---

## 系统架构

```
┌──────────────────────────────────────────────────────────────┐
│                  TCP 服务器 (L2 行情数据源)                     │
│               逐笔委托端口: 18103 | 逐笔成交端口: 18105         │
└────────────────────────┬─────────────────────────────────────┘
                         │
                         ▼
                ┌─────────────────┐
                │ L2TcpSubscriber │  ← 登录认证、订阅股票、接收循环
                └────────┬─────────┘
                         │
                         ▼
                ┌─────────────────┐
                │  L2Parser 解析   │  ← 字段分割、数据验证、创建事件
                └────────┬─────────┘
                         │
          ┌──────────────┴──────────────┐
          ▼                             ▼
    ┌──────────┐                 ┌──────────┐
    │ 委托事件  │                 │ 成交事件  │
    └─────┬────┘                 └─────┬────┘
          │                             │
          └──────────────┬──────────────┘
                         ▼
                ┌─────────────────┐
                │   OrderBook     │  ← MPSC 无锁队列、处理线程
                │                 │     维护买卖盘深度、订单管理
                └────────┬─────────┘
                         │
                         ▼
                ┌─────────────────┐
                │  SendServer     │  ← 命名管道推送交易信号
                │  (Named Pipe)   │     to_python_pipe
                └──────────────────┘
```

---

## 快速开始

### 环境要求

| 组件 | 版本 | 说明 |
|-----|------|------|
| **操作系统** | Windows 10/11 | 需支持命名管道和 Winsock2 |
| **编译器** | MSVC 2019+ / MinGW-w64 | 需支持 C++17 标准 |
| **CMake** | 3.29+ | 构建系统生成器 |

### 构建步骤

```powershell
# 克隆仓库
git clone <your-repo-url>
cd l2-realtime-market-engine

# 创建构建目录
mkdir build
cd build

# 生成构建文件
cmake ..

# 编译项目（Release 模式）
cmake --build . --config Release
```

### 配置文件

编辑 `bin/config.ini`：

```ini
[server]
tcp_host = www.l2api.cn
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

## 核心模块

### 1. OrderBook（订单簿引擎）

**职责**：维护股票的实时买卖盘深度，处理委托和成交事件

**关键特性**：
- 使用 `std::map` 维护价格档位的有序结构（买盘降序、卖盘升序）
- 使用 `std::list` 实现同价位订单的 FIFO 队列
- 使用 `std::unordered_map` 实现订单 ID 到迭代器的 O(1) 快速查找
- 采用 MPSC（多生产者单消费者）无锁队列接收事件
- 独立线程处理事件，避免阻塞数据接收

**核心方法**：
```cpp
void pushEvent(const MarketEvent& event);     // 压入实时事件到队列
void pushHistoryEvent(const MarketEvent& e);  // 压入历史事件到队列
void handleOrderEvent(const MarketEvent& e);  // 处理委托事件
void handleTradeEvent(const MarketEvent& e);  // 处理成交事件
void checkLimitUpWithdrawal();                // 检查涨停撤单
```

**实现文件**：
- 头文件：[include/OrderBook.h](include/OrderBook.h)
- 源文件：[src/OrderBook.cpp](src/OrderBook.cpp)

---

### 2. L2TcpSubscriber（TCP 订阅客户端）

**职责**：建立与 L2 数据服务器的 TCP 连接，订阅逐笔行情数据流

**关键特性**：
- 基于 Winsock2 的高性能网络通信
- 独立接收线程持续监听数据
- 自动重连机制
- 支持同时订阅多个股票

**核心方法**：
```cpp
bool connect();                               // 连接服务器
void login();                                 // 登录认证
void subscribe(const std::string& symbol);    // 订阅股票
void receiveLoop();                           // 接收循环（后台线程）
```

**实现文件**：
- 头文件：[include/L2TcpSubscriber.h](include/L2TcpSubscriber.h)
- 源文件：[src/L2TcpSubscriber.cpp](src/L2TcpSubscriber.cpp)

---

### 3. L2HttpDownloader（HTTP 下载器）

**职责**：从 HTTP 接口下载历史逐笔数据，用于回测和初始化

**关键特性**：
- 基于 cpp-httplib 的 HTTP 客户端
- 支持 Base64 + GZIP 解压
- 异步下载多个数据文件
- 自动解析并推送到 `OrderBook`

**核心方法**：
```cpp
void login();                                            // 登录获取 Cookie
void download_and_parse(const std::string& symbol,       // 下载并解析数据
                        const std::string& type);
```

**实现文件**：
- 头文件：[include/L2HttpDownloader.h](include/L2HttpDownloader.h)
- 源文件：[src/L2HttpDownloader.cpp](src/L2HttpDownloader.cpp)

---

### 4. SendServer & ReceiveServer（命名管道通信）

**职责**：通过 Windows 命名管道与外部系统（Python/Node.js）进行双向通信

**SendServer**：
- 推送交易信号到外部系统
- 异步非阻塞发送
- 格式：`<股票代码#价格,数量,账户ID>`

**ReceiveServer**：
- 接收外部系统的订阅请求
- 动态创建 OrderBook 实例
- 格式：`<股票代码#价格,数量,账户ID>`

**实现文件**：
- [include/SendServer.h](include/SendServer.h) / [src/SendServer.cpp](src/SendServer.cpp)
- [include/ReceiveServer.h](include/ReceiveServer.h) / [src/ReceiveServer.cpp](src/ReceiveServer.cpp)

---

## 数据格式说明

### 逐笔委托（Order）数据格式

```
<index,symbol,time,num1,price,volume,type,side,num2,num3,channel#>
```

| 字段 | 类型 | 说明 | 示例 |
|-----|------|------|------|
| index | int | 推送序号 | 157 |
| symbol | string | 股票代码 | 600895.SH |
| time | int | 时间戳 (HHMMSSmmm) | 93025100 |
| price | int | 价格（单位：0.0001元） | 190000 (19.00元) |
| volume | int | 数量（单位：股） | 100 |
| type | int | 委托类型 | 2 (1=市价, 2=限价, 10=撤单) |
| side | int | 买卖方向 | 1 (1=买入, 2=卖出) |

### 逐笔成交（Trade）数据格式

```
<index,symbol,time,num1,price,volume,amount,side,type,sell_id,buy_id#>
```

| 字段 | 类型 | 说明 |
|-----|------|------|
| sell_id | int | 卖方委托序号 |
| buy_id | int | 买方委托序号 |
| amount | int | 成交金额（单位：0.0001元） |

**完整定义**：参见 [include/DataStruct.h](include/DataStruct.h) 中的 `L2Order` 和 `L2Trade` 结构

---

## 性能优化

### 优化技术

1. **无锁并发队列**：`moodycamel::BlockingConcurrentQueue` 实现 MPSC 模式
2. **零拷贝解析**：使用 `std::string_view` 避免字符串拷贝
3. **快速整数转换**：`std::from_chars`（比 `std::stoi` 快 3-5 倍）
4. **高效数据结构**：
   - `std::map` 用于价格档位：O(log n) 插入/删除
   - `std::unordered_map` 用于订单查找：O(1) 平均复杂度
   - `std::list` 用于同价位订单：O(1) 插入/删除
5. **历史数据去重**：使用 `std::unordered_set` 避免重复处理

### 性能指标（参考值）

- **事件处理速度**：> 100,000 events/s（单股票）
- **内存占用**：~50MB（单股票 + 10万订单）
- **端到端延迟**：< 1ms（事件入队到处理完成）
- **订单查找**：O(1) 时间复杂度

---

## 项目结构

```
l2-realtime-market-engine/
├─ main.cpp                    # 程序入口
├─ CMakeLists.txt              # CMake 构建配置
├─ LICENSE                     # MIT 许可证
├─ README.md                   # 本文件
│
├─ include/                    # 头文件目录
│  ├─ OrderBook.h             # 订单簿核心类
│  ├─ L2TcpSubscriber.h       # TCP 订阅客户端
│  ├─ L2HttpDownloader.h      # HTTP 下载器
│  ├─ L2Parser.h              # 数据解析器（Header-Only）
│  ├─ DataStruct.h            # 数据结构定义（Header-Only）
│  ├─ SendServer.h            # 命名管道发送服务
│  ├─ ReceiveServer.h         # 命名管道接收服务
│  ├─ Logger.h                # 日志系统接口
│  ├─ ConfigReader.h          # INI 配置文件读取（Header-Only）
│  ├─ Base64Decoder.h         # Base64 + GZIP 解压（Header-Only）
│  └─ ReadFile.h              # CSV 文件读取（Header-Only）
│
├─ src/                        # 源文件目录
│  ├─ OrderBook.cpp
│  ├─ L2TcpSubscriber.cpp
│  ├─ L2HttpDownloader.cpp
│  ├─ SendServer.cpp
│  ├─ ReceiveServer.cpp
│  └─ Logger.cpp
│
├─ third_party/                # 第三方库
│  └─ include/
│     ├─ concurrentqueue/     # 无锁并发队列
│     ├─ httplib/             # HTTP 库
│     ├─ nlohmann/            # JSON 库
│     ├─ spdlog/              # 日志库
│     └─ zlib/                # 压缩库
│
├─ bin/                        # 输出目录
│  ├─ main.exe                # 可执行文件
│  ├─ config.ini              # 配置文件
│  ├─ data/                   # 历史数据存储
│  │  ├─ 20260106_Order_000592.SZ.csv
│  │  └─ 20260106_Tran_000592.SZ.csv
│  └─ logs/                   # 日志文件
│
└─ build/                      # CMake 构建目录
```

---

## 使用示例

### 示例 1：订阅实时行情

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
    std::string host = config.get("server", "tcp_host");
    int order_port = config.getInt("server", "order_port");
    
    // 创建订单簿容器
    auto orderBooksPtr = std::make_shared<
        std::unordered_map<std::string, std::unique_ptr<OrderBook>>
    >();
    
    // 创建订阅器
    L2TcpSubscriber orderSub(
        host, order_port, username, password, "order", orderBooksPtr
    );
    
    // 连接并订阅股票
    if (orderSub.connect()) {
        std::string symbol = "600895.SH";
        (*orderBooksPtr)[symbol] = std::make_unique<OrderBook>(
            symbol, nullptr, nullptr
        );
        orderSub.subscribe(symbol);
    }
    
    std::cin.get();
    return 0;
}
```

### 示例 2：下载历史数据

```cpp
#include "L2HttpDownloader.h"

// 创建下载器
L2HttpDownloader downloader(http_url, username, password, orderBooksPtr);

// 下载并解析历史数据（自动推送到 OrderBook）
downloader.download_and_parse("600895.SH", "Order");  // 逐笔委托
downloader.download_and_parse("600895.SH", "Tran");   // 逐笔成交
```

### 示例 3：与 Python 集成

**C++ 端（发送交易信号）**：
```cpp
auto sendServer = std::make_shared<SendServer>("to_python_pipe");
sendServer->send("<600895.SH#19.00,1000,account123>");
```

**Python 端（接收信号）**：
```python
import win32pipe
import win32file

pipe = win32file.CreateFile(
    r'\\.\pipe\to_python_pipe',
    win32file.GENERIC_READ,
    0, None,
    win32file.OPEN_EXISTING,
    0, None
)

while True:
    result, data = win32file.ReadFile(pipe, 4096)
    signal = data.decode('utf-8')
    print(f"收到信号: {signal}")
    # 解析并执行交易逻辑...
```

完整示例：参见 [main.cpp](main.cpp)

---

## 故障排查

### 常见问题

**Q: 编译错误 - 'std::from_chars' 未找到**  
A: 确保在 `CMakeLists.txt` 中启用 C++17 标准：
```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

**Q: WSAStartup 失败，错误代码 10093**  
A: 链接 Winsock2 库：
```cmake
target_link_libraries(common PRIVATE ws2_32)
```

**Q: 命名管道连接失败**  
A: 检查管道名称格式：`\\.\pipe\pipe_name`，确保服务端先启动

**Q: 日志文件无法创建**  
A: 确保 `bin/logs/` 目录存在，或在代码中自动创建：
```cpp
std::filesystem::create_directories("logs");
```

**Q: 历史数据下载失败**  
A: 检查网络连接和认证信息，查看日志文件了解详细错误

**Q: OrderBook 处理速度慢**  
A: 
- 检查是否开启了 Release 模式编译（`-O2` 或 `-O3`）
- 减少日志输出频率
- 确认无锁队列配置正确

---

## 技术亮点

### 1. 高性能并发设计

- **无锁队列**：采用 Cameron314 的 `BlockingConcurrentQueue`，避免互斥锁开销
- **MPSC 模式**：多个数据源（委托/成交）并发写入，单线程顺序处理
- **线程隔离**：网络接收、数据解析、订单簿更新分离，互不阻塞

### 2. 精确的订单簿重构

- **事件溯源**：完整记录所有委托和成交事件
- **严格校验**：成交必须匹配对应的委托订单
- **去重机制**：历史数据自动去重，避免重复处理
- **深度维护**：实时计算买卖盘各档位的价格和数量

### 3. 智能信号检测

- **涨停撤单监控**：检测涨停价撤单行为，识别潜在交易机会
- **大单追踪**：监控特定账户的大额委托
- **封单分析**：计算涨停/跌停封单量变化
- **自定义策略**：灵活接入外部策略系统

### 4. 企业级日志系统

- 基于 `spdlog` 的异步日志
- 支持多级别（DEBUG/INFO/WARN/ERROR）
- 自动文件滚动和压缩
- 高性能无阻塞写入

---

## 开发路线图

- [x] 基础订单簿引擎
- [x] TCP 实时数据订阅
- [x] HTTP 历史数据下载
- [x] 命名管道通信
- [x] 涨停撤单监控
- [ ] WebSocket 推送支持
- [ ] RESTful API 接口
- [ ] 多股票并发优化
- [ ] 数据持久化（SQLite）
- [ ] 回测框架集成
- [ ] 跨平台支持（Linux/macOS）

---

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

---

## 致谢

本项目使用了以下优秀的开源库：

- [moodycamel/concurrentqueue](https://github.com/cameron314/concurrentqueue) - 高性能无锁并发队列
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) - 轻量级 C++ HTTP 库
- [nlohmann/json](https://github.com/nlohmann/json) - 现代 C++ JSON 库
- [gabime/spdlog](https://github.com/gabime/spdlog) - 超快的 C++ 日志库
- [zlib](https://www.zlib.net/) - 数据压缩库

---

<div align="center">

**专为量化交易打造 🚀**

如有问题或建议，欢迎提交 [Issue](../../issues)

</div>
