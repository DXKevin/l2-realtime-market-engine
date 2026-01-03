// L2Parser.h
#pragma once

#include <charconv>
#include <string>
#include <string_view>
#include <vector>
#include <cctype>

#include "Logger.h"
#include "DataStruct.h"


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
    static constexpr size_t field_num = 11; // 11 个字段
};

template<>
struct L2FieldCount<L2Trade> {
    static constexpr size_t field_num = 12; // 11 个字段
};


inline std::vector<MarketEvent> parseL2Data(std::string_view data, std::string_view type) {
    constexpr size_t ORDER_FIELDS  = L2FieldCount<L2Order>::field_num;
    constexpr size_t TRADE_FIELDS  = L2FieldCount<L2Trade>::field_num;

    std::vector<MarketEvent> event_list;
    size_t pos = 0;

    while (pos < data.size()) {
        // 查找 '<'
        size_t open = data.find('<', pos);
        if (open == std::string_view::npos) break;

        // 查找 '>'
        size_t close = data.find('>', open);
        if (close == std::string_view::npos) break;

        // 提取整个 <...> 内容
        std::string_view full_record = data.substr(open + 1, close - open - 1);
        // 按 '#' 分割为多个订单
        std::string_view current = full_record;
        size_t start = 0;
        size_t end = 0;

        while ((end = current.find('#', start)) != std::string_view::npos) {
            std::string_view order_part = current.substr(start, end - start);
            if (!order_part.empty()) {
                auto fields = splitByComma(order_part);
                if (type == "order"){
                    if (fields.size() == ORDER_FIELDS) {
                        event_list.emplace_back(L2Order(fields));
                    } else {
                        LOG_WARN("L2Parser", "order字段数不匹配");
                    }
                } else if (type == "trade"){
                    if (fields.size() == TRADE_FIELDS) {
                        event_list.emplace_back(L2Trade(fields));
                    } else {
                        LOG_WARN("L2Parser", "trade字段数不匹配");
                    }
                }
            }
            start = end + 1;  // 跳过 '#'
        }

        // 处理最后一个 # 后的部分（如果没有 #，就是整个 full_record）
        // if (start < current.size()) {
        //     std::string_view last_order = current.substr(start);
        //     if (!last_order.empty()) {
        //         auto fields = splitByComma(last_order);
        //         if (fields.size() == EXPECTED_FIELDS) {
        //             data_list.emplace_back(constructor(fields));
        //         } else {
        //             LOG_WARN("L2Parser", "字段数不匹配");
        //         }
        //     }
        // }

        pos = close + 1; // 移动到 '>' 之后
    }

    return event_list;
}


inline std::string formatStockAccount(
    const std::string& stock_symbol,
    const std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> stock_with_accounts_ptr
) {
    auto it = stock_with_accounts_ptr->find(stock_symbol);

    if (it == stock_with_accounts_ptr->end()) {
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
    const std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> stockWithAccountsPtr
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
    (*stockWithAccountsPtr)[std::string(symbol)] = std::move(result); 

    return std::string(symbol);
}