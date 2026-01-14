#pragma once
#include <string>
#include <string_view>
#include <vector>
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

inline int timeIntToMs(int time_int) {
    int H = time_int / 10000000;
    int M = (time_int / 100000) % 100;
    int S = (time_int / 1000) % 100;
    int ms = time_int % 1000;

    return (H * 3600 + M * 60 + S) * 1000 + ms;
}

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

inline long long svToLong(std::string_view sv) {
    if (sv.empty()) {
        return 0;
    }

    long long result = 0;
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
    //数据原始字段
    int index;          // 推送序号
    std::string symbol; // 合约代码
    int time; // 时间
    int num1; // 委托编号
    int price;          // 单位：0.0001 元（即 price / 10000.0 为真实价格）
    int volume;         // 股数
    int type;           // 10 = 撤单, 1 = 市价, 2 = 限价, 3 = 本方最优
    int side;           // 1 = buy, 2 = sell
    int num2; // 原始订单号, 仅上交所
    int num3;       // 逐笔数据序号, 仅上交所
    int channel;    // 交易通道号

    // 以下为衍生字段
    int id; //无论是上交所还是深交所，统一使用这个字段
    int timestamp; // 时间戳，单位毫秒

    L2Order() = default;
    // 构造函数：从 string_view 拷贝
    L2Order(const std::vector<std::string_view>& fields) 
        : index(svToInt(fields[0]))
        , symbol(fields[1])
        , time(svToInt(fields[4]))
        , num1(svToInt(fields[5]))
        , price(svToInt(fields[6]))
        , volume(svToInt(fields[7]))
        , type(svToInt(fields[8]))
        , side(svToInt(fields[9]))
        , num2(svToInt(fields[10]))
        , num3(svToInt(fields[11]))
        , channel(svToInt(fields[12]))
    {
        if (!symbol.empty() && symbol[0] == '6') {
            // 上海票用num2
            id = num2;
        } else {
            // 深圳票用num1
            id = num1;
        }
        timestamp = timeIntToMs(time);
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
    // 数据原始字段
    int index;          // 推送序号
    std::string symbol; // 合约代码
    int time; // 时间
    int num1; // 成交编号
    int price;          // 单位：0.0001 元（即 price / 10000.0 为真实价格）
    int volume;         // 股数
    long long amount;         // 成交金额, 因为价格一般是一万倍表示, 所以金额可能很大, 用 long long 存储
    int side;           // 1 = buy, 2 = sell
    int type;           // 0 = 成交, 1 = 撤单 
    int sell_id;        // 卖方委托号
    int buy_id;         // 买方委托号

    // 以下为衍生字段
    int timestamp; // 时间戳，单位毫秒

    L2Trade() = default;
    // 构造函数：从 string_view 拷贝
    L2Trade(const std::vector<std::string_view>& fields) 
        : index(svToInt(fields[0]))
        , symbol(fields[1])
        , time(svToInt(fields[4]))
        , num1(svToInt(fields[5]))
        , price(svToInt(fields[6]))
        , volume(svToInt(fields[7]))
        , amount(svToLong(fields[8]))
        , side(svToInt(fields[9]))
        , type(svToInt(fields[10]))
        , sell_id(svToInt(fields[12]))
        , buy_id(svToInt(fields[13]))
    {
        timestamp = timeIntToMs(time);
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

struct MarketSpot {
    int index;          // 推送序号
    std::string symbol; // 合约代码
    std::string time; // 时间
    int status; // 交易状态
    int last_close_price; // 昨收价
    int open_price; // 今开盘
    int high_price; // 最高价
    int low_price; // 最低价
    int last_price; // 最新价
    int ask1_price; // 卖一价
    int ask2_price; // 卖二价
    int ask3_price; // 卖三价
    int ask4_price; // 卖四价
    int ask5_price; // 卖五价
    int ask6_price; // 卖六价
    int ask7_price; // 卖七价
    int ask8_price; // 卖八价
    int ask9_price; // 卖九价
    int ask10_price; // 卖十价
    int ask1_volume; // 卖一量
    int ask2_volume; // 卖二量
    int ask3_volume; // 卖三量
    int ask4_volume; // 卖四量
    int ask5_volume; // 卖五量
    int ask6_volume; // 卖六量
    int ask7_volume; // 卖七量
    int ask8_volume; // 卖八量
    int ask9_volume; // 卖九量
    int ask10_volume; // 卖十量
    int bid1_price; // 买一价
    int bid2_price; // 买二价
    int bid3_price; // 买三价
    int bid4_price; // 买四价
    int bid5_price; // 买五价
    int bid6_price; // 买六价
    int bid7_price; // 买七价
    int bid8_price; // 买八价
    int bid9_price; // 买九价
    int bid10_price; // 买十价
    int bid1_volume; // 买一量
    int bid2_volume; // 买二量
    int bid3_volume; // 买三量
    int bid4_volume; // 买四量
    int bid5_volume; // 买五量
    int bid6_volume; // 买六量
    int bid7_volume; // 买七量
    int bid8_volume; // 买八量
    int bid9_volume; // 买九量
    int bid10_volume; // 买十量
    int transaction_number; // 成交笔数
    int transaction_volume; // 成交总量
    int transaction_amount; // 成交总金额
    int order_buy_volume; // 委买总量
    int order_sell_volume; // 委卖总量
    int avg_price; // 加权平均价
    int limit_up_price; // 涨停价
    int limit_down_price; // 跌停价
    int transaction_buy_number; // 买入笔数 *仅限上交所
    int transaction_sell_number; // 卖出笔数 *仅限上交所
    int transaction_buy_cancel_number; // 买入撤单笔数 *仅限上交所
    int transaction_buy_cancel_volume; // 买入撤单量 *仅限上交所
    int transaction_sell_cancel_number; // 卖出撤单笔数 *仅限上交所
    int transaction_sell_cancel_volume; // 卖出撤单量 *仅限上交所
};

struct MarketEvent {
    enum class EventType { ORDER, TRADE, MARKET_SPOT } type;

    std::variant< L2Order, L2Trade, MarketSpot > data;

    MarketEvent() = default;
    MarketEvent(const L2Order& o) : type(EventType::ORDER), data(o) {}
    MarketEvent(const L2Trade& t) : type(EventType::TRADE), data(t) {}
    MarketEvent(const MarketSpot& m) : type(EventType::MARKET_SPOT), data(m) {}

};