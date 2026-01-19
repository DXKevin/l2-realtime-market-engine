# L2 Realtime Market Engine 🚀

**高性能 Level-2 证券行情实时处理引擎**

[![C++17](https://img.shields.io/badge/C++-17-blue.svg?style=flat&logo=c%2B%2B)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows-0078D6.svg?style=flat&logo=windows)](https://www.microsoft.com/windows)
[![License](https://img.shields.io/badge/License-MIT-green.svg?style=flat)](LICENSE)

---

## 📖 项目简介
**L2 Realtime Market Engine** 是一款专为量化交易设计的**低延迟、高并发**行情处理引擎。它支持上海（SSE）与深圳（SZSE）证券交易所的 Level-2 逐笔委托及成交数据，通过现代 C++ 17 技术栈实现微秒级的数据解析与订单簿（OrderBook）重构。

---

## ✨ 核心特性
*   ⚡ **极致性能**: 基于无锁并发队列（Lock-free Queue），实现单线程 10w+ events/s 处理能力。
*   🏗️ **精准建模**: 完整重构 L2 订单簿，支持实时市场深度（Market Depth）计算。
*   🔗 **跨语言集成**: 通过 Windows 命名管道（Named Pipe）实现与 Python/Node.js 的无损实时通信。
*   📥 **全场景数据**: 支持 TCP 实时订阅与 HTTP 历史数据下载，满足回测与实盘需求。
*   🧠 **智能监控**: 内置涨停撤单、大单追踪、封单分析等量化信号生成器。

---

## 🏗️ 系统架构
```mermaid
graph TD
    A[L2 Data Source] -->|TCP| B[L2TcpSubscriber]
    A -->|HTTP| C[L2HttpDownloader]
    B -->|Lock-free Queue| D[OrderBook Engine]
    C -->|Batch Push| D
    D -->|Real-time Signals| E[Named Pipe Server]
    E -->|Signal Push| F[Python/Node.js Logic]
```

---

## 🚀 快速开始
### 环境要求
*   **操作系统**: Windows 10/11
*   **编译器**: MSVC 2019+ / MinGW-w64 (需支持 C++17)
*   **构建工具**: CMake 3.29+

### 构建与运行
```powershell
# 克隆并进入目录
git clone <your-repo-url>
cd l2-realtime-market-engine

# 编译项目
mkdir build && cd build
cmake ..
cmake --build . --config Release

# 运行程序
cd ../bin
./main.exe
```

---

## 📊 性能指标
| 指标 | 表现 | 备注 |
| :--- | :--- | :--- |
| **单路吞吐量** | > 100,000 events/s | i7-12700K 参考环境 |
| **处理延迟** | < 500 μs | 事件入队至 OrderBook 更新 |
| **内存效率** | ~50MB / 股票 | 包含 10 万档活跃订单 |

---

## 💡 技术亮点
1.  **无锁并发设计**: 引入 `moodycamel::BlockingConcurrentQueue` 消除互斥锁开销，最大化多核性能。
2.  **高性能字符串解析**: 使用 `std::string_view` 与 `std::from_chars` 实现零拷贝/快速整数转换，比常规方法快 3-5 倍。
3.  **创新的双向管道**: 采用命名管道实现跨进程通信，确保 C++ 核心引擎与上层 Python/Node.js 策略层解耦的同时保持低延迟。

---

## 📁 项目结构
*   `include/`: 核心类定义及 Header-only 工具类。
*   `src/`: 核心模块实现（Subscriber, OrderBook, PipeServer 等）。
*   `third_party/`: 内置高性能第三方库依赖。
*   `bin/`: 可执行文件、配置文件及日志输出目录。

---

## 🤝 开源致谢
本项目受益于以下优秀的开源项目：
*   [moodycamel/concurrentqueue](https://github.com/cameron314/concurrentqueue) - 高性能无锁并发队列。
*   [gabime/spdlog](https://github.com/gabime/spdlog) - 极速 C++ 日志库。
*   [yhirose/cpp-httplib](https://github.com/yhirose/cpp-httplib) - 轻量级 HTTP 客户端。

---

## 📜 许可证
本项目采用 [MIT License](LICENSE)。

---

<div align="center">

**专为量化交易打造 🚀**

如有问题或建议，欢迎提交 Issue 或 Pull Request！

</div>
