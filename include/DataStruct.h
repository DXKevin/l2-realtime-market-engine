#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <charconv>
#include <variant>
#include "Logger.h"

inline int timeStrToInt(const std::string& time_str) {
    if (time_str.empty()) return -1;
    if (time_str.size() > 9 || time_str.size() < 8) return -1;

    // 步骤2: 手动解析 HH, MM, SS, mmm（避免 stoi，更安全高效）
    auto charToInt = [](char c) -> int {
        // if (c >= '0' && c <= '9') return c - '0';
        // return -1; // 非数字

        return c - '0';
    };

    // 解析一位数（位置 i 和 i+1）
    auto parse1 = [&](int i) -> int {
        int d1 = charToInt(time_str[i]);
        if (d1 == -1) return -1;
        return d1;
    };

    // 解析两位数（位置 i 和 i+1）
    auto parse2 = [&](int i) -> int {
        int d1 = charToInt(time_str[i]);
        int d2 = charToInt(time_str[i+1]);
        if (d1 == -1 || d2 == -1) return -1;
        return d1 * 10 + d2;
    };

    // 解析三位数（位置 i, i+1, i+2）
    auto parse3 = [&](int i) -> int {
        int d1 = charToInt(time_str[i]);
        int d2 = charToInt(time_str[i+1]);
        int d3 = charToInt(time_str[i+2]);
        if (d1 == -1 || d2 == -1 || d3 == -1) return -1;
        return d1 * 100 + d2 * 10 + d3;
    };

    auto checktime = [](int H, int M, int S, int ms) -> int {
        if (H < 0 || H > 23) return -1;
        if (M < 0 || M > 59) return -1;
        if (S < 0 || S > 59) return -1;
        if (ms < 0 || ms > 999) return -1;
        
        return (H * 3600 + M * 60 + S) * 1000 + ms;
    };  

    if (time_str.size() == 9) {
        int H = parse2(0); // HH
        int M = parse2(2); // MM
        int S = parse2(4); // SS
        int ms = parse3(6); // mmm

        return checktime(H, M, S, ms);
    } else {
        int H = parse1(0); // HH
        int M = parse2(1); // MM
        int S = parse2(3); // SS
        int ms = parse3(5); // mmm

        return checktime(H, M, S, ms);
    }
}


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

struct L2Order {
    int index;          // 推送序号
    std::string symbol; // 合约代码
    std::string time; // 时间
    std::string num1; // 委托编号
    int price;          // 单位：0.0001 元（即 price / 10000.0 为真实价格）
    int volume;         // 股数
    int type;           // 10 = 撤单, 1 = 市价, 2 = 限价, 3 = 本方最优
    int side;           // 1 = buy, 2 = sell
    std::string num2; // 原始订单号, 仅上交所
    int num3;       // 逐笔数据序号, 仅上交所
    int channel;    // 交易通道号

    std::string id; //无论是上交所还是深交所，统一使用这个字段
    int timestamp; // 时间戳，单位毫秒

    L2Order() = default;
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
        , num2(fields[8])
        , num3(svToInt(fields[9]))
        , channel(svToInt(fields[10]))
    {
        if (!symbol.empty() && symbol[0] == '6') {
            // 上海票用num2
            id = num2;
        } else {
            // 深圳票用num1
            id = num1;
        }
        timestamp = timeStrToInt(time);
    }

    void info() const {
        LOG_INFO("L2Order", "股票代码:{}, 时间:{}, 价格:{}, 数量:{}, 委托类型:{}, 方向:{}, 委托编号:{}, 时间戳:{}",
        symbol,
        time,
        price,
        volume,
        type,
        side,
        id,
        timestamp
        );
    };

};

struct L2Trade {
    int index;          // 推送序号
    std::string symbol; // 合约代码
    std::string time; // 时间
    std::string num1; // 成交编号
    int price;          // 单位：0.0001 元（即 price / 10000.0 为真实价格）
    int volume;         // 股数
    std::string amount;         // 成交金额
    int side;           // 1 = buy, 2 = sell
    int type;           // 0 = 成交, 1 = 撤单 
    std::string channel; // 频道代码
    std::string sell_id;        // 卖方委托号
    std::string buy_id;         // 买方委托号

    int timestamp; // 时间戳，单位毫秒

    L2Trade() = default;
    // 构造函数：从 string_view 拷贝
    L2Trade(const std::vector<std::string_view>& fields) 
        : index(svToInt(fields[0]))
        , symbol(fields[1])
        , time(fields[2])
        , num1(fields[3])
        , price(svToInt(fields[4]))
        , volume(svToInt(fields[5]))
        , amount(fields[6])
        , side(svToInt(fields[7]))
        , type(svToInt(fields[8]))
        , channel(fields[9])
        , sell_id(fields[10])
        , buy_id(fields[11])
    {
        timestamp = timeStrToInt(time);
    }

    void info() const {
        LOG_INFO("L2Trade", "股票代码:{}, 时间:{}, 价格:{}, 成交量:{}, 成交金额:{}, 方向:{}, 成交类型:{}, 卖方委托号:{}, 买方委托号:{}, 时间戳:{}",
        symbol,
        time,
        price,
        volume,
        amount,
        side,
        type,
        sell_id,
        buy_id,
        timestamp
        );

    };

};


struct MarketEvent {
    enum class EventType { ORDER, TRADE } type;

    std::variant< L2Order, L2Trade > data;

    MarketEvent() = default;
    MarketEvent(const L2Order& o) : type(EventType::ORDER), data(o) {}
    MarketEvent(const L2Trade& t) : type(EventType::TRADE), data(t) {}

    
};