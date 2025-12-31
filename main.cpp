#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

#include "logger.h"
#include "L2TcpSubscriber.h"
#include "ConfigReader.h"
#include "L2Parser.h"
#include "OrderBook.h"

static const char* module_name = "Main";

std::unordered_map<std::string, std::unique_ptr<OrderBook>> g_orderbooks;


int main() {
    try{
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8); // 设置控制台为UTF-8编码以支持中文输出，防止exe运行时命令行输出乱码
#endif

        init_log_system("logs/app.log");
        ConfigReader config("config.ini");

        std::string host = config.get("server", "host");
        int order_port = config.getInt("server", "order_port");
        int trade_port = config.getInt("server", "trade_port");

        std::string username = config.get("auth", "username");
        std::string password = config.get("auth", "password");
        LOG_INFO(module_name, "配置文件加载完成");

        // std::string symbol = "600376.SH";
        std::string symbol = "002050.SZ";
        g_orderbooks[symbol] = std::make_unique<OrderBook>(symbol);

        L2TcpSubscriber OrderSubscriber(host, order_port, username, password, "order", &g_orderbooks);
        L2TcpSubscriber TradeSubscriber(host, trade_port, username, password, "trade", &g_orderbooks);

        if (OrderSubscriber.connect()) {
            OrderSubscriber.subscribe(symbol);
        }

        if (TradeSubscriber.connect()) {
            TradeSubscriber.subscribe(symbol);
        }

        // std::string test_data = "<157,600693.SH,103659530,14494293,190000,100,2,2,9100941,14494293,2,#><158,600693.SH,103659530,14494299,182000,100,2,1,9100946,14494299,2,#><159,600693.SH,103659570,14494369,182100,400,2,1,9100980,14494369,2,#><160,600693.SH,103659600,14494454,190000,2400,2,2,9101018,14494454,2,#><161,600693.SH,103659620,14494490,187000,100,2,2,9101037,14494490,2,#>";
        // auto events = parseL2Data(test_data, "order");

        // for (const auto& event : events) {
        //     auto it = g_orderbooks.find(symbol);
        //     if (it != g_orderbooks.end()) {
        //         it->second->pushEvent(event);
        //     } else {
        //         LOG_WARN(module_name, "未找到对应的 OrderBook 处理数据，合约代码: {}", symbol);
        //     }
        // }

        std::cin.get();

    } catch (const std::exception& ex) {
        std::cerr << "异常终止: " << ex.what() << std::endl;
        return -1;
    }
}