// logger.h
// 日志系统 - 基于spdlog的多模块日志管理
#pragma once

#include <spdlog/spdlog.h>
#include <memory>

/**
 * @brief 初始化日志系统
 * @param filename 日志文件路径，默认为 "logs/app.log"
 * @return 初始化是否成功
 * 
 * 说明：
 * - 创建控制台和文件两个输出目标
 * - 支持按日期分割日志文件
 * - 线程安全
 */
bool init_log_system(const char* filename = "logs/app.log");

/**
 * @brief 获取指定模块的日志器
 * @param module_name 模块名称
 * @return 模块专用的日志器
 * 
 * 说明：
 * - 如果模块日志器不存在，会自动创建
 * - 线程安全
 */
std::shared_ptr<spdlog::logger> get_module_logger(const char* module_name);

// 快捷宏（在任何cpp文件中使用）
#define LOG_INFO(module, ...)    get_module_logger(module)->info(__VA_ARGS__)
#define LOG_WARN(module, ...)    get_module_logger(module)->warn(__VA_ARGS__)
#define LOG_ERROR(module, ...)   get_module_logger(module)->error(__VA_ARGS__)
#define LOG_DEBUG(module, ...)   get_module_logger(module)->debug(__VA_ARGS__)