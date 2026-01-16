// L2Parser.h
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <winscard.h>

#include "Logger.h"
#include "DataStruct.h"
#include "FileOperator.h"


// 辅助函数：按 ',' 分割 string_view（不支持转义）
inline std::vector<std::string_view> splitByComma(std::string_view str) {
    std::vector<std::string_view> tokens;
    size_t start = 0;
    for (size_t i = 0; i <= str.size(); ++i) {
        if (i == str.size() || str[i] == ',') {
            auto token = str.substr(start, i - start);
            // 忽略空字段（处理 trailing comma）
            if (!token.empty()) {
                tokens.push_back(token);
            }
            start = i + 1;
        }
    }
    return tokens;
}


template<typename T>
struct L2FieldCount {
    static constexpr size_t field_num = 0; // 默认值
};

template<>
struct L2FieldCount<L2Order> {
    static constexpr size_t field_num = 13; // 14 个字段
};

template<>
struct L2FieldCount<L2Trade> {
    static constexpr size_t field_num = 14; // 14 个字段
};


inline std::vector<MarketEvent> parseL2Data(
    std::string_view data, std::string_view type, std::string& buffer_) {
    constexpr size_t ORDER_FIELDS  = L2FieldCount<L2Order>::field_num;
    constexpr size_t TRADE_FIELDS  = L2FieldCount<L2Trade>::field_num;

    std::vector<MarketEvent> event_list;
    size_t pos = 0;
    
    buffer_.append(data.data(), data.size());
    std::string_view buffer_view = buffer_;

    while (pos < buffer_.size()) {
        // 查找 '<'
        size_t open = buffer_view.find('<', pos);
        if (open == std::string_view::npos) {
            buffer_.clear(); // 没有找到 '<'，清空缓冲区
            buffer_.append(data.data(), data.size()); // 保留未处理数据
            
            LOG_WARN("L2Parser", "无法找到 '<', 丢弃缺失数据, 保留新缓冲区数据: {}", buffer_);
            return event_list;
        }

        // 查找 '>'
        size_t close = buffer_view.find('>', open);
        if (close == std::string_view::npos) {
            buffer_ = buffer_.substr(open); // 保留不完整部分

            // LOG_WARN("L2Parser", "无法找到 '>', 保留不完整数据到缓冲区: {}", buffer_);
            return event_list;
        }
        
        // 提取整个 <...> 内容
        std::string_view full_record = buffer_view.substr(open + 1, close - open - 1);
        
        // 按 '#' 分割为多个订单
        size_t start = 0;
        size_t end = 0;

        auto splitfunc = [&](std::string_view order_part){
            if (!order_part.empty()) {
                auto fields = splitByComma(order_part);
                if (type == "order"){
                    if (fields.size() == ORDER_FIELDS) {
                        event_list.emplace_back(L2Order(fields));
                    } else {
                        LOG_WARN("L2Parser", "buffer_:{}, open:{}, length:{}", buffer_, open, close-open-1);
                        LOG_WARN("L2Parser", "order字段数不匹配, data:{}", full_record);
                    }
                } else if (type == "trade"){
                    if (fields.size() == TRADE_FIELDS) {
                        event_list.emplace_back(L2Trade(fields));
                    } else {
                        LOG_WARN("L2Parser", "buffer_:{}, open:{}, length:{}", buffer_, open, close-open-1);
                        LOG_WARN("L2Parser", "trade字段数不匹配, data:{}", full_record);
                    }
                }
            }
        };

        while (true) {
            end = full_record.find("#", start);
            if (end == std::string_view::npos) {
                if (start >= full_record.size()) {
                    break;
                }

                std::string_view order_part = full_record.substr(start);
                if (type == "order") {
                    writeTxtFile("tcp_order_data.txt", std::string(order_part));
                } else if (type == "trade") {
                    writeTxtFile("tcp_trade_data.txt", std::string(order_part));
                }
                splitfunc(order_part);
                break;
            } else {
                std::string_view order_part = full_record.substr(start, end - start);
                if (type == "order") {
                    writeTxtFile("tcp_order_data.txt", std::string(order_part));
                } else if (type == "trade") {
                    writeTxtFile("tcp_trade_data.txt", std::string(order_part));
                }
                splitfunc(order_part);
                start = end + 1;  // 跳过 '#'
            }
        }

        pos = close + 1; // 移动到 '>' 之后
    }
    buffer_.clear(); // 全部处理完，清空缓冲区
    return event_list;
}


inline std::string formatStockAccount(
    const std::string& stock_symbol,
    const std::unordered_map<std::string, std::vector<std::string>>& stockWithAccounts_ref
) {
    auto it = stockWithAccounts_ref.find(stock_symbol);

    if (it == stockWithAccounts_ref.end()) {
        return "";
    }

    std::string result = "<" + stock_symbol + "#";

    const auto& accounts = it->second;
    for (size_t i = 0; i < accounts.size(); ++i) {
        if (i > 0) {
            result += ",";
        }

        result += accounts[i];

    }

    result += ">";

    return result;
}

inline std::string parseAndStoreStockAccount(
    const std::string_view message,
    std::unordered_map<std::string, std::vector<std::string>>& stockWithAccounts_ref
) {
    if (message.size() < 3 || message.front() != '<' || message.back() != '>') {
        // 格式不合法
        LOG_WARN("L2Parser", "收到格式不合法的消息: {}", message);
        return "";
    }

    // 去掉 < 和 >
    std::string_view content = message.substr(1, message.size() - 2);

    // 查找 '#' 的位置
    size_t pos = content.find('#');
    if (pos == std::string_view::npos) {
        LOG_WARN("L2Parser", "收到格式不合法的消息, 缺少 #: {}", message);
        return "";
    }

    std::string_view symbol = content.substr(0, pos);
    std::string_view accountsStr = content.substr(pos + 1);

    std::vector<std::string> result;
    auto accounts_views = splitByComma(accountsStr);
    for (const auto& acc_view : accounts_views) {
        result.emplace_back(acc_view);
    }

    // 写入共享 map（注意：多线程需加锁！）
    stockWithAccounts_ref[std::string(symbol)] = std::move(result); 

    return std::string(symbol);
}