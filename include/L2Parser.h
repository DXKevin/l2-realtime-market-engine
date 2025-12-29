// L2Parser.h
#ifndef L2_PARSER_H
#define L2_PARSER_H

#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <functional>

namespace {

// 辅助函数：安全地将 string_view 转为 int（仅用于小整数）
int svToInt(std::string_view sv) {
    if (sv.empty()) return 0;
    int result = 0;
    bool neg = false;
    size_t i = 0;
    if (sv[0] == '-') {
        neg = true;
        ++i;
    }
    for (; i < sv.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(sv[i]))) break;
        result = result * 10 + (sv[i] - '0');
    }
    return neg ? -result : result;
}

// 辅助函数：按 ',' 分割 string_view（不支持转义）
std::vector<std::string_view> splitByComma(std::string_view str) {
    std::vector<std::string_view> tokens;
    size_t start = 0;
    for (size_t i = 0; i <= str.size(); ++i) {
        if (i == str.size() || str[i] == ',') {
            tokens.emplace_back(str.substr(start, i - start));
            start = i + 1;
        }
    }
    return tokens;
}

} // anonymous namespace


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
std::vector<T> parseL2Data(std::string_view data, std::function<T(const std::vector<std::string_view>&)> constructor) {
    
    std::vector<T> data_list;
    size_t pos = 0;

    while (pos < data.size()) {
        // 查找 '<'
        size_t open = data.find('<', pos);
        if (open == std::string_view::npos) break;
        
        // 查找 '#>'
        size_t close = data.find("#>", open);
        if (close == std::string_view::npos) break;

        // 提取内容：跳过 '<'，到 '#' 之前
        std::string_view record = data.substr(open + 1, close - open - 1);
        pos = close + 2; // move past "#>"
        
        // 分割字段
        auto fields = splitByComma(record);
        if (fields.size() < 11) continue; // 至少11个字段

        data_list.emplace_back(constructor(fields));
    }

    return data_list;
}

#endif // L2_PARSER_H