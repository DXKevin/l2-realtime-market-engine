#pragma once

#include <spdlog/spdlog.h>
#include <memory>

bool init_log_system(const char* filename = "logs/app.log");

// 获取模块的日志器
std::shared_ptr<spdlog::logger> get_module_logger(const char* module_name);

// 快捷宏（在任何cpp文件中使用）
#define LOG_INFO(module, ...)    get_module_logger(module)->info(__VA_ARGS__)
#define LOG_WARN(module, ...)    get_module_logger(module)->warn(__VA_ARGS__)
#define LOG_ERROR(module, ...)   get_module_logger(module)->error(__VA_ARGS__)
#define LOG_DEBUG(module, ...)   get_module_logger(module)->debug(__VA_ARGS__)