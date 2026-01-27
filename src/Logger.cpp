#include "Logger.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>

#include <spdlog/async.h> 
#include <spdlog/details/thread_pool.h>

#include <unordered_map>
#include <mutex>
#include <filesystem>
#include <iostream>

// 声明两个共享的 sink（输出目标）
static std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> g_console_sink = nullptr;
static std::shared_ptr<spdlog::sinks::daily_file_sink_mt> g_file_sink = nullptr;

// 存储所有模块的日志器
static std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> g_module_loggers;
static std::mutex g_loggers_mutex;  // 创建互斥锁，保护多线程安全

// 记录是否已初始化异步线程池
static bool g_async_pool_initialized = false;

// 初始化日志系统
bool init_log_system(const char* filename) {
    try {
        if (!g_async_pool_initialized) {
            // 队列大小，例如 8192；后台线程数量，例如 1
            // 队列大小需要根据你的日志吞吐量调整，如果日志量很大，可能需要更大。
            // 后台线程数一般 1 个就够了，除非你有多个 sink 且 I/O 非常重。
            spdlog::init_thread_pool(8192, 1); 
            g_async_pool_initialized = true;
        }

        // 1. 创建日志目录
        std::filesystem::path filepath(filename);
        std::filesystem::create_directories(filepath.parent_path());
        
        // 2. 创建控制台 sink（输出到屏幕）
        g_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        g_console_sink->set_level(spdlog::level::debug);
        
        // 3. 创建每日文件 sink（输出到文件）
        g_file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
            filename, 0, 0, false, 5
        );
        g_file_sink->set_level(spdlog::level::debug);
        
        // 4. 设置日志格式
        g_console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] [thread %t] %v");
        g_file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [thread %t] %v");
        
        // std::cout << "日志系统初始化成功！日志保存在: " << filename << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "初始化日志失败: " << e.what() << std::endl;
        return false;
    }
}

// 获取模块的日志器
std::shared_ptr<spdlog::logger> get_module_logger(const char* module_name) {
    std::lock_guard<std::mutex> lock(g_loggers_mutex); // 确保线程安全
    
    // 检查是否已经创建过
    auto it = g_module_loggers.find(module_name);
    if (it != g_module_loggers.end()) {
        return it->second;
    }
    
    try {
        // 创建同步日志器
        // auto new_logger = std::make_shared<spdlog::logger>(
        //     module_name, 
        //     spdlog::sinks_init_list{g_console_sink, g_file_sink}
        // );

        // 创建异步日志器
        auto new_logger = std::make_shared<spdlog::async_logger>(
            module_name,
            spdlog::sinks_init_list{g_console_sink, g_file_sink},
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block // 溢出时阻塞
        );

        // 设置日志级别
        new_logger->set_level(spdlog::level::debug);
        
        // 保存起来
        g_module_loggers[module_name] = new_logger;
        
        return new_logger;
        
    } catch (...) {
        // 出错时返回一个安全的日志器
        return spdlog::default_logger();
    }
}