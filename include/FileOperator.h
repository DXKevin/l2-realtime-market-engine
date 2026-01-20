#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <filesystem>

#include "Logger.h"

inline bool safe_localtime(std::time_t time, std::tm& result_tm) {
#ifdef _WIN32 // Windows
    // localtime_s returns errno_t, 0 on success
    return localtime_s(&result_tm, &time) == 0;
#else // POSIX (Linux, macOS, etc.)
    // localtime_r returns pointer to result_tm on success, nullptr on failure
    return localtime_r(&time, &result_tm) != nullptr;
#endif
}

inline std::string readCsvFile(const std::string& filePath) {
    std::ifstream file(filePath);

    if (!file.is_open()) {
        LOG_ERROR("ReadFile", "无法打开文件: {}", filePath);
        return "";
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}


inline void writeTxtFile(const std::string& filePath, const std::string& content) {

    std::ofstream outfile(filePath, std::ios::app | std::ios::out);

    if (!outfile.is_open()) {
        LOG_ERROR("WriteFile", "无法打开文件: {}", filePath);
        return;
    }
    outfile << content;
    outfile.close();
}

inline std::vector<std::string> readTxtFile(const std::string& filePath) {

    std::ifstream infile(filePath);

    if (!infile.is_open()) {
        LOG_ERROR("ReadFile", "无法打开文件: {}", filePath);
        return {};
    }
    
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(infile, line)) {
        lines.push_back(line);
    }
    infile.close();
    return lines;
}

inline bool deleteFile(const std::string& filepath) {
    if (!std::filesystem::exists(filepath)) {
        LOG_WARN("FileOperator", "文件不存在: {}", filepath);
        return false;
    }
    try {
        if (std::filesystem::remove(filepath)) {
            return true;
        } else {
            LOG_ERROR("FileOperator", "删除文件失败: {}", filepath);
            return false;
        }
    } catch (const std::filesystem::filesystem_error& ex) {
        LOG_ERROR("FileOperator", "删除文件失败: {}", ex.what());
        return false;
    }
}

inline bool deleteIfBeforeTimePeriod(const std::string& filepath, int hour, int minute) {
    // 获取当前系统时间
    auto now = std::chrono::system_clock::now();
    std::time_t time_t_now = std::chrono::system_clock::to_time_t(now);

    std::tm local_tm;
    if (!safe_localtime(time_t_now, local_tm)) {
        LOG_ERROR("FileOperator", "获取当前时间结构体失败");
        return false;
    }

    int now_hour = local_tm.tm_hour;
    int now_minute = local_tm.tm_min;

    // 判断是否在 09:10 之前
    if (now_hour < hour || (now_hour == hour && now_minute < minute)) {
        return deleteFile(filepath);
    }

    // LOG_INFO("FileOperator", "文件修改时间不是 {}:{} 之前，不删除文件: {}", hour, minute, filepath);
    return false;
}

inline bool deleteIfModifiedYesterday(const std::string& filepath) {
    if (!std::filesystem::exists(filepath)) {
        LOG_WARN("FileOperator", "文件不存在: {}", filepath);
        return false;
    }

    // 获取文件修改时间
    std::filesystem::file_time_type ftime;
    try {
        ftime = std::filesystem::last_write_time(filepath);
    } catch (const std::filesystem::filesystem_error& ex) {
        LOG_ERROR("FileOperator", "获取文件修改时间失败: {}", ex.what());
        return false;
    }

    // 将文件修改时间转换为time_t
    auto sys_time_point = ftime.time_since_epoch();
    auto sys_time_point_as_duration = std::chrono::duration_cast<std::chrono::system_clock::duration>(sys_time_point);
    auto file_time_sys = std::chrono::system_clock::time_point(sys_time_point_as_duration);

    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    auto today_time_t = std::chrono::system_clock::to_time_t(now);

    // 获取昨天时间转换为tm结构体
    auto yesterday_time_t = today_time_t - 86400;
    std::tm yesterday_tm{};
    if (!safe_localtime(yesterday_time_t, yesterday_tm)) {
        LOG_ERROR("FileOperator", "获取昨天时间失败");
        return false;
    }

    // 将文件修改时间转换为tm结构体
    std::time_t file_time_t = std::chrono::system_clock::to_time_t(file_time_sys);
    std::tm file_tm{};
    if (!safe_localtime(file_time_t, file_tm)) {
        LOG_ERROR("FileOperator", "获取文件时间失败");
        return false;
    }

    // 判断文件修改时间是否是昨天
    bool is_yesterday = (file_tm.tm_year == yesterday_tm.tm_year &&
                         file_tm.tm_mon == yesterday_tm.tm_mon &&
                         file_tm.tm_mday == yesterday_tm.tm_mday);

    if (is_yesterday) {
        try {
            if (std::filesystem::remove(filepath)) {
                return true;
            } else {
                LOG_ERROR("FileOperator", "删除文件失败: {}", filepath);
                return false;
            }
        } catch (const std::filesystem::filesystem_error& ex) {
            LOG_ERROR("FileOperator", "删除文件失败: {}", ex.what());
            return false;
        }
    } else {
        LOG_INFO("FileOperator", "文件修改时间不是昨天: {}", filepath);
        return false;
    }
}