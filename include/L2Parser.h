// L2Parser.h
#ifndef L2_PARSER_H
#define L2_PARSER_H

#include <charconv>
#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <functional>

#include "Logger.h"



// 辅助函数：安全地将 string_view 转为 int（仅用于小整数）
inline int svToInt(std::string_view sv) {
    if (sv.empty()) {
        return 0;
    }

    int result = 0;
    std::from_chars_result res = std::from_chars(sv.data(), sv.data() + sv.size(), result);

    // 如果解析失败（如包含非数字字符），返回 0
    // 你也可以选择抛出异常或记录日志，根据需求调整
    if (res.ec == std::errc::invalid_argument || res.ec == std::errc::result_out_of_range) {
        // 可选：记录警告
        LOG_WARN("L2Parser", "Invalid integer: {}", sv);
        return 0;
    }

    return result;
}

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


struct L2Order {
    int index;          // 推送序号
    std::string symbol; // 合约代码
    std::string time; // 时间
    std::string num1; // 委托编号
    int price;          // 单位：0.0001 元（即 price / 10000.0 为真实价格）
    int volume;         // 股数
    int type;           // 10 = 撤单, 1 = 市价, 2 = 限价, 3 = 本方最优
    int side;           // 1 = buy, 2 = sell
    std::string id; // 原始订单号, 仅上交所
    int num2;       // 逐笔数据序号, 仅上交所
    int channel;    // 交易通道号

    // 构造函数：从 string_view 拷贝
    L2Order(const std::vector<std::string_view>& fields) 
        : index(svToInt(fields[0]))
        , symbol(fields[1])
        , time(fields[2])
        , num1(fields[3])
        , price(svToInt(fields[4]))
        , volume(svToInt(fields[5]))
        , type(svToInt(fields[6]))
        , side(svToInt(fields[7]))
        , id(fields[8])
        , num2(svToInt(fields[9]))
        , channel(svToInt(fields[10]))
    {}

};

struct L2Trade {
    int index;          // 推送序号
    std::string symbol; // 合约代码
    std::string time; // 时间
    std::string num1; // 成交编号
    int price;          // 单位：0.0001 元（即 price / 10000.0 为真实价格）
    int volume;         // 股数
    int amount;         // 成交金额
    int side;           // 1 = buy, 2 = sell
    int type;           // 0 = 成交, 1 = 撤单 
    int sell_id;        // 卖方委托号
    int buy_id;         // 买方委托号

    // 构造函数：从 string_view 拷贝
    L2Trade(const std::vector<std::string_view>& fields) 
        : index(svToInt(fields[0]))
        , symbol(fields[1])
        , time(fields[2])
        , num1(fields[3])
        , price(svToInt(fields[4]))
        , volume(svToInt(fields[5]))
        , amount(svToInt(fields[6]))
        , side(svToInt(fields[7]))
        , type(svToInt(fields[8]))
        , sell_id(svToInt(fields[9]))
        , buy_id(svToInt(fields[10]))
    {}
};


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
    static constexpr size_t field_num = 11; // 11 个字段
};

template<typename T>
std::vector<T> parseL2Data(std::string_view data) {
    constexpr size_t EXPECTED_FIELDS = L2FieldCount<T>::field_num;
    std::vector<T> data_list;
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

                if (fields.size() == EXPECTED_FIELDS) {
                    data_list.emplace_back(fields);
                } else {
                    LOG_WARN("L2Parser", "字段数不匹配");
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

    return data_list;
}

#endif // L2_PARSER_H