// ConfigReader.h
#pragma once
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>

class ConfigReader {
public:
    explicit ConfigReader(const std::string& filename) {
        load(filename);
    }

    std::string get(const std::string& section, const std:: string& key, const std::string& default_val = "") const {
        auto it = data_.find(section + "." + key);
        return (it != data_.end()) ? it->second : default_val;
    }

    int getInt(const std::string& section, const std::string& key, int default_val = 0) const {
        std::string val = get(section, key);
        if (val.empty()) return default_val;
        try {
            return std::stoi(val);
        } catch (...) {
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