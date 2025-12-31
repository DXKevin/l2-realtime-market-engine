// ConfigReader.h
// 配置文件读取器 - 支持INI格式配置文件的读取和解析
#pragma once
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>

/**
 * @class ConfigReader
 * @brief INI格式配置文件读取器
 * 
 * 支持标准INI格式:
 * [section]
 * key = value
 * ; 注释
 * 
 * 使用示例:
 * ConfigReader config("config.ini");
 * std::string host = config.get("server", "host");
 * int port = config.getInt("server", "port", 8080);
 */
class ConfigReader {
public:
    /**
     * @brief 构造函数，加载配置文件
     * @param filename 配置文件路径
     * @throws std::runtime_error 如果文件无法打开
     */
    explicit ConfigReader(const std::string& filename) {
        load(filename);
    }

    /**
     * @brief 获取字符串类型的配置值
     * @param section 配置节名称
     * @param key 配置键名称
     * @param default_val 默认值（如果键不存在）
     * @return 配置值或默认值
     */
    std::string get(const std::string& section, const std::string& key, const std::string& default_val = "") const {
        auto it = data_.find(section + "." + key);
        return (it != data_.end()) ? it->second : default_val;
    }

    /**
     * @brief 获取整数类型的配置值
     * @param section 配置节名称
     * @param key 配置键名称
     * @param default_val 默认值（如果键不存在或转换失败）
     * @return 配置值或默认值
     */
    int getInt(const std::string& section, const std::string& key, int default_val = 0) const {
        std::string val = get(section, key);
        if (val.empty()) return default_val;
        try {
            return std::stoi(val);
        } catch (const std::exception&) {
            return default_val;
        }
    }

private:
    void load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("无法打开配置文件: " + filename);
        }

        std::string line;
        std::string current_section;

        while (std::getline(file, line)) {
            // 去除注释
            size_t comment_pos = line.find(';');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }

            // 去除首尾空格
            trim(line);
            if (line.empty()) continue;

            // 检查是否是 [section]
            if (line.front() == '[' && line.back() == ']') {
                current_section = line.substr(1, line.size() - 2);
                trim(current_section);
                continue;
            }

            // 解析 key = value
            size_t eq_pos = line.find('=');
            if (eq_pos == std::string::npos) continue;

            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            trim(key);
            trim(value);

            // 去掉值两侧的可选引号 ("..." 或 '...')
            if (value.size() >= 2) {
                if ((value.front() == '"' && value.back() == '"') ||
                    (value.front() == '\'' && value.back() == '\'')) {
                    value = value.substr(1, value.size() - 2);
                }
            }
            if (!key.empty() && !current_section.empty()) {
                data_[current_section + "." + key] = value;
            }
        }
    }

    static void trim(std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), s.end());
    }

    std::unordered_map<std::string, std::string> data_;
};