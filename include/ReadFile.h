#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "Logger.h"

std::string readCsvFile(const std::string& filePath) {
    std::ifstream file(filePath);

    if (!file.is_open()) {
        LOG_ERROR("ReadFile", "无法打开文件: {}", filePath);
        return "";
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}