// L2Parser.h
// L2行情数据解析器 - 解析上交所/深交所L2行情数据流
#pragma once

#include <charconv>
#include <string>
#include <string_view>
#include <vector>
#include <cctype>

#include "logger.h"
#include "DataStruct.h"


/**
 * @brief 按逗号分割字符串视图
 * @param str 待分割的字符串
 * @return 分割后的字符串视图数组
 * 
 * 注意：不支持转义字符
 */
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

/**
 * @brief L2数据字段数量模板
 * 用于编译期确定不同数据类型的字段数
 */
template<typename T>
struct L2FieldCount {
    static constexpr size_t field_num = 0; // 默认值
};

template<>
struct L2FieldCount<L2Order> {
    static constexpr size_t field_num = 11;
};

template<>
struct L2FieldCount<L2Trade> {
    static constexpr size_t field_num = 12;
};


/**
 * @brief 解析L2行情数据流
 * @param data 原始行情数据，格式: <field1,field2,...#field1,field2,...#>
 * @param type 数据类型："order" 或 "trade"
 * @return 解析后的市场事件列表
 * 
 * 数据格式说明：
 * - 使用 < 和 > 包裹一个或多个记录
 * - 记录之间用 # 分隔
 * - 记录内字段用逗号分隔
 * - 示例: <1,600376.SH,103659530,14494293,190000,100,2,2,9100941,14494293,2,#>
 */
inline std::vector<MarketEvent> parseL2Data(std::string_view data, std::string_view type) {
    constexpr size_t ORDER_FIELDS  = L2FieldCount<L2Order>::field_num;
    constexpr size_t TRADE_FIELDS  = L2FieldCount<L2Trade>::field_num;

    std::vector<MarketEvent> event_list;
    
    if (data.empty()) {
        return event_list;
    }
    
    if (type != "order" && type != "trade") {
        LOG_WARN("L2Parser", "Unknown type: {}", std::string(type));
        return event_list;
    }

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
                        LOG_WARN("L2Parser", "order字段数不匹配: expected {}, got {}", ORDER_FIELDS, fields.size());
                    }
                } else if (type == "trade"){
                    if (fields.size() == TRADE_FIELDS) {
                        event_list.emplace_back(L2Trade(fields));
                    } else {
                        LOG_WARN("L2Parser", "trade字段数不匹配: expected {}, got {}", TRADE_FIELDS, fields.size());
                    }
                }
            }
            start = end + 1;  // 跳过 '#'
        }

        pos = close + 1; // 移动到 '>' 之后
    }

    return event_list;
}