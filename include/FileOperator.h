#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

#include "Logger.h"

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
    outfile << content << std::endl;
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


