# l2-realtime-market-engine

L2 实时行情处理系统（C++）

## 项目简介

本项目是一个高性能的L2（Level 2）实时股票行情处理引擎，支持上交所和深交所的逐笔委托和逐笔成交数据。系统采用多线程架构，能够高效处理实时行情数据流，并维护完整的订单簿。

## 主要特性

- ✅ **多市场支持**：兼容上交所和深交所的L2行情数据格式
- ✅ **高性能处理**：采用无锁并发队列，支持高吞吐量数据处理
- ✅ **实时订单簿**：维护买卖盘口的实时订单簿，支持价格档位聚合
- ✅ **乱序处理**：智能处理乱序到达的委托和成交事件
- ✅ **线程安全**：完整的多线程支持，线程安全的事件处理
- ✅ **跨平台**：支持Windows和Linux平台（添加了平台兼容性处理）

## 系统架构

```
┌─────────────────┐
│  L2行情服务器   │
└────────┬────────┘
         │ TCP
         ↓
┌─────────────────┐
│ L2TcpSubscriber │ ← 接收行情数据
└────────┬────────┘
         │
         ↓ 解析
┌─────────────────┐
│    L2Parser     │ ← 解析数据流
└────────┬────────┘
         │
         ↓ 分发
┌─────────────────┐
│   OrderBook     │ ← 维护订单簿
│  (多线程处理)    │
└─────────────────┘
```

## 主要组件

### 1. ConfigReader
INI格式配置文件读取器，支持：
- 标准INI格式解析
- 类型安全的配置读取（string/int）
- 默认值支持

### 2. L2TcpSubscriber
TCP客户端，负责：
- 连接L2行情服务器
- 用户认证
- 订阅股票行情
- 接收和分发数据

### 3. L2Parser
高性能数据解析器：
- 零拷贝解析（使用string_view）
- 支持批量数据解析
- 字段验证

### 4. OrderBook
订单簿管理器：
- 多线程事件处理
- 价格档位聚合
- 乱序事件处理
- 市价单特殊处理

### 5. Logger
基于spdlog的日志系统：
- 多模块日志支持
- 控制台和文件双输出
- 线程安全

## 构建要求

- CMake 3.29.0+
- C++17 编译器
- spdlog (第三方库)
- concurrentqueue (第三方库，已包含)

## 构建方法

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## 配置文件示例

创建 `config.ini` 文件：

```ini
[server]
host = "your.server.com"
order_port = 8001
trade_port = 8002

[auth]
username = "your_username"
password = "your_password"
```

## 使用示例

```cpp
#include "L2TcpSubscriber.h"
#include "OrderBook.h"
#include "ConfigReader.h"

// 初始化日志系统
init_log_system("logs/app.log");

// 加载配置
ConfigReader config("config.ini");
std::string host = config.get("server", "host");
int order_port = config.getInt("server", "order_port");

// 创建订单簿
std::unordered_map<std::string, std::unique_ptr<OrderBook>> orderbooks;
orderbooks["600376.SH"] = std::make_unique<OrderBook>("600376.SH");

// 创建订阅客户端
L2TcpSubscriber subscriber(host, order_port, username, password, "order", &orderbooks);

// 连接并订阅
if (subscriber.connect()) {
    subscriber.subscribe("600376.SH");
}
```

## 数据格式

### 逐笔委托格式
```
<index,symbol,time,order_id,price,volume,type,side,num2,num3,channel,#>
```

### 逐笔成交格式
```
<index,symbol,time,trade_id,price,volume,amount,side,type,channel,sell_id,buy_id,#>
```

## 代码质量改进

本项目经过全面的代码质量评估和改进：

### 修复的问题
1. **跨平台兼容性**：修复了Windows特定代码，添加了平台保护宏
2. **头文件命名**：统一了头文件命名规范（logger.h）
3. **输入验证**：添加了全面的输入验证和边界检查
4. **错误处理**：改进了错误处理和日志记录
5. **内存安全**：添加了空指针检查和资源清理
6. **代码文档**：为所有公共API添加了详细注释

### 添加的验证
- 空字符串检查
- 数值范围验证
- 重复订单检查
- Socket状态验证
- 队列大小监控

## 许可证

详见 LICENSE 文件

## 贡献

欢迎提交Issue和Pull Request！
