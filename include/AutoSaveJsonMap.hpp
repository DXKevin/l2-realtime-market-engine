#pragma once
#include <fstream>
#include <map>
#include <list>
#include <string>
#include <shared_mutex>

#include "Logger.h"
#include "nlohmann/json.hpp"

template<typename Key = std::string, typename Value = std::list<int>>
class AutoSaveJsonMap {
public:
    using MapType = std::map<Key, Value>;

    // 构造函数：指定文件名，自动加载
    explicit AutoSaveJsonMap(const std::string& filename = "stock_account.json")
        : filename_(filename) {
        load();
    }

    // 写操作：独占锁
    void set(const Key& key, Value value) {
        std::unique_lock<std::shared_mutex> lock(mtx_);
        data_[key] = std::move(value);
        saveUnsafe();
    }   

    // 读操作：共享锁（多个读可并发）
    std::optional<Value> get(const Key& key) const {
        std::shared_lock<std::shared_mutex> lock(mtx_);
        auto it = data_.find(key);
        if (it != data_.end()) return it->second;
        return std::nullopt;
    }

    bool contains(const Key& key) const {
        std::shared_lock<std::shared_mutex> lock(mtx_);
        return data_.find(key) != data_.end();
    }

    void forEach(const std::function<void(const Key&, const Value&)>& f) const {
        std::shared_lock<std::shared_mutex> lock(mtx_);
        for (const auto& kv : data_) f(kv.first, kv.second);
    }

private:
    mutable std::shared_mutex mtx_;
    std::string filename_;
    MapType data_;

    void load() {
        std::ifstream file(filename_);
        if (!file.is_open()) {
            // std::cout << "首次运行，创建新数据文件: " << filename_ << "\n";
            return;
        }
        try {
            nlohmann::json j;
            file >> j;
            for (auto& [k, v] : j.items()) {
                data_[k] = v.get<Value>();
            }
            // std::cout << "已加载 " << data_.size() << " 条记录\n";
        } catch (const std::exception& e) {
            LOG_ERROR("AutoSaveJsonMap", "JSON 解析失败，使用空数据: {}", e.what());
        }
    }

    void saveUnsafe() {
        nlohmann::json j;
        for (const auto& [k, v] : data_) {
            j[k] = v;
        }
        std::ofstream file(filename_);
        if (file.is_open()) {
            file << j.dump(4);
        } else {
            LOG_ERROR("AutoSaveJsonMap", "无法写入文件: {}", filename_);
        }
    }
};