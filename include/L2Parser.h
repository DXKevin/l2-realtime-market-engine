// L2Parser.h
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <winscard.h>

#include "Logger.h"
#include "DataStruct.h"
#include "AutoSaveJsonMap.hpp"
#include "AsyncFileWriter.h"


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
    std::string& data, DataMessage::MessageType type, 
    std::string& buffer_, AsyncFileWriter& asyncFileWriter_ref) {

    constexpr size_t ORDER_FIELDS  = L2FieldCount<L2Order>::field_num;
    constexpr size_t TRADE_FIELDS  = L2FieldCount<L2Trade>::field_num;

    std::vector<MarketEvent> event_list;
    size_t pos = 0;
    
    buffer_.append(data);
    std::string_view buffer_view = buffer_;

    while (pos < buffer_.size()) {
        // 查找 '<'
        size_t open = buffer_view.find('<', pos);
        if (open == std::string_view::npos) {
            buffer_.clear(); // 没有找到 '<'，清空缓冲区
            buffer_.append(data); // 保留未处理数据
            
            LOG_WARN("L2Parser", "无法找到 '<', 丢弃缺失数据, 保留新缓冲区数据: {}", buffer_);
            return event_list;
        }

        // 查找 '>'
        size_t close = buffer_view.find('>', open);
        if (close == std::string_view::npos) {
            buffer_ = buffer_.substr(open); // 保留不完整部分

            LOG_WARN("L2Parser", "无法找到 '>', 保留不完整数据到缓冲区: {}", buffer_);
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
                if (type == DataMessage::MessageType::ORDER){
                    if (fields.size() == ORDER_FIELDS) {
                        MarketEvent event{L2Order(fields)};
                        event_list.push_back(event);

                        std::string symbol = std::get<L2Order>(event.data).symbol;
                        std::string path = "data/" + symbol + "_order_tcp.txt";
                        asyncFileWriter_ref.write_async(path, std::string(order_part));

                    } else {
                        LOG_WARN("L2Parser", "buffer_:{}, open:{}, length:{}", buffer_, open, close-open-1);
                        LOG_WARN("L2Parser", "order字段数不匹配, data:{}", full_record);
                    }
                } else if (type == DataMessage::MessageType::TRADE){
                    if (fields.size() == TRADE_FIELDS) {
                        MarketEvent event{L2Trade(fields)};
                        event_list.push_back(event);

                        std::string symbol = std::get<L2Trade>(event.data).symbol;
                        std::string path = "data/" + symbol + "_trade_tcp.txt";
                        asyncFileWriter_ref.write_async(path, std::string(order_part));
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

                if (order_part.find("HeartBeat") != std::string_view::npos ||
                    order_part.find("DY2") != std::string_view::npos ||
                    order_part.find("Order") != std::string_view::npos ||
                    order_part.find("Tran") != std::string_view::npos ||
                    order_part.find("Login") != std::string_view::npos) {
                    break;
                }

                splitfunc(order_part);
                break;
            } else {
                std::string_view order_part = full_record.substr(start, end - start);

                if (order_part.find("HeartBeat") != std::string_view::npos ||
                    order_part.find("DY2") != std::string_view::npos ||
                    order_part.find("Order") != std::string_view::npos ||
                    order_part.find("Tran") != std::string_view::npos ||
                    order_part.find("Login") != std::string_view::npos) {
                    break;
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

inline std::string formatCancelMessage(
    const std::string& stock_symbol,
    const AutoSaveJsonMap<std::string, std::vector<int>>& cancelMonitorInfo_ref
) {
    if (!cancelMonitorInfo_ref.contains(stock_symbol)) {
        return "";
    }

    auto accounts_opt = cancelMonitorInfo_ref.get(stock_symbol);
    if (!accounts_opt || accounts_opt->empty()) {
        LOG_ERROR("L2Parser", "无法获取股票代码 {} 的撤单监控信息或者为空", stock_symbol);
        return "";
    }

    std::string result = "<" + stock_symbol + "#";
    const auto& accounts = *accounts_opt;
    for (size_t i = 0; i < accounts.size(); ++i) {
        if (i > 0) result += ",";
        result += std::to_string(accounts[i]);
    }
    result += "#cancel>";
    return result;
}

inline std::string formatSellMessage(
    const std::string& stock_symbol,
    const AutoSaveJsonMap<std::string, std::unordered_map<int, int>>& sellMonitorInfo_ref
) {
    if (!sellMonitorInfo_ref.contains(stock_symbol)) {
        return "";
    }

    auto map_opt = sellMonitorInfo_ref.get(stock_symbol);
    if (!map_opt || map_opt->empty()) {
        LOG_ERROR("L2Parser", "无法获取股票代码 {} 的卖出监控信息或者为空", stock_symbol);
        return "";
    }

    std::string result = "<" + stock_symbol + "#";
    bool first = true;
    for (const auto& [accountId, percentage] : *map_opt) {
        if (!first) result += ",";
        first = false;
        result += std::to_string(accountId) + "s" + std::to_string(percentage);
    }
    result += "#sell>";
    return result;
}


inline std::string parseAndStoreStockAccount(
    const std::string_view message,
    AutoSaveJsonMap<std::string, std::vector<int>>& cancelMonitorInfo_ref,
    AutoSaveJsonMap<std::string, std::unordered_map<int, int>>& sellMonitorInfo_ref  // 注意：这里变了！
) {
    LOG_INFO("L2Parser", "收到消息: {}", message);
    if (message.size() < 3 || message.front() != '<' || message.back() != '>') {
        LOG_WARN("L2Parser", "收到格式不合法的消息: {}", message);
        return "";
    }

    std::string_view content = message.substr(1, message.size() - 2);

    size_t firstHash = content.find('#');
    if (firstHash == std::string_view::npos) {
        LOG_WARN("L2Parser", "消息缺少第一个 #: {}", message);
        return "";
    }

    std::string symbol(content.substr(0, firstHash));
    std::string_view rest = content.substr(firstHash + 1);

    size_t secondHash = rest.find('#');
    if (secondHash == std::string_view::npos) {
        LOG_WARN("L2Parser", "消息缺少第二个 #: {}", message);
        return "";
    }

    std::string accountsStr(rest.substr(0, secondHash));
    std::string typeTag(rest.substr(secondHash + 1));

    auto accountViews = splitByComma(accountsStr);

    if (typeTag == "cancel") {
        std::vector<int> newAccounts;
        for (const auto& accView : accountViews) {
            if (accView.empty()) continue;
            int acc = svToInt(accView);
            if (acc != 0 || accView == "0") {
                newAccounts.push_back(acc);
            } else {
                LOG_WARN("L2Parser", "无法解析 cancel 账号: {}", accView);
            }
        }

        // 合并去重（保持原有逻辑）
        if (cancelMonitorInfo_ref.contains(symbol)) {
            auto existingOpt = cancelMonitorInfo_ref.get(symbol);
            if (existingOpt) {
                auto& existing = *existingOpt;
                for (int acc : newAccounts) {
                    if (std::find(existing.begin(), existing.end(), acc) == existing.end()) {
                        existing.push_back(acc);
                    }
                }
                cancelMonitorInfo_ref.set(symbol, existing);
            } else {
                cancelMonitorInfo_ref.set(symbol, newAccounts);
            }
        } else {
            cancelMonitorInfo_ref.set(symbol, newAccounts);
        }

    } else if (typeTag == "sell") {
        std::unordered_map<int, int> newEntries; // accountId -> percentage

        for (const auto& accView : accountViews) {
            if (accView.empty()) continue;

            size_t sPos = accView.find('s');
            if (sPos == std::string_view::npos || sPos == 0 || sPos == accView.size() - 1) {
                LOG_WARN("L2Parser", "sell 账号格式错误（缺少 's' 或格式不对）: {}", accView);
                continue;
            }

            std::string_view idPart = accView.substr(0, sPos);
            std::string_view pctPart = accView.substr(sPos + 1);

            int accountId = svToInt(idPart);
            int percentage = svToInt(pctPart);

            if ((accountId == 0 && idPart != "0") || (percentage == 0 && pctPart != "0")) {
                LOG_WARN("L2Parser", "sell 账号解析失败: {}", accView);
                continue;
            }

            if (percentage < 0 || percentage > 100) {
                LOG_WARN("L2Parser", "卖出比例超出范围 [0,100]: {}", percentage);
                // 可选择 clamp 或跳过，这里跳过
                continue;
            }

            newEntries[accountId] = percentage;
        }

        // 获取现有数据，合并（新覆盖旧）
        std::unordered_map<int, int> merged;
        if (sellMonitorInfo_ref.contains(symbol)) {
            auto existingOpt = sellMonitorInfo_ref.get(symbol);
            if (existingOpt) {
                merged = std::move(*existingOpt);
            }
        }

        // 用新数据覆盖
        for (const auto& [id, pct] : newEntries) {
            merged[id] = pct;
        }

        sellMonitorInfo_ref.set(symbol, merged);

    } else {
        LOG_WARN("L2Parser", "未知的消息类型标签: {}, 消息: {}", typeTag, message);
        return "";
    }

    return symbol;
}