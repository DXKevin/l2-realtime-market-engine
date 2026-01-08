#include <iostream>
#include <memory>

#include "Logger.h"
#include "L2TcpSubscriber.h"
#include "ConfigReader.h"
#include "L2Parser.h"
#include "OrderBook.h"
#include "SendServer.h"
#include "ReceiveServer.h"




int main() {
    static const char* module_name = "Main";

    try {
        // 初始化设置
        SetConsoleOutputCP(CP_UTF8); // 设置控制台为UTF-8编码以支持中文输出，防止exe运行时命令行输出乱码

        // 初始化日志系统
        init_log_system("logs/app.log");

        // 读取配置文件
        ConfigReader config("config.ini");

        std::string host = config.get("server", "host");
        int order_port = config.getInt("server", "order_port");
        int trade_port = config.getInt("server", "trade_port");

        std::string username = config.get("auth", "username");
        std::string password = config.get("auth", "password");
        LOG_INFO(module_name, "配置文件加载完成");

        // 初始化全局数据结构: orderBooksPtr存放股票代码到OrderBook实例的映射, stockWithAccountsPtr存放股票代码到账户列表的映射
        auto orderBooksPtr = std::make_shared<std::unordered_map<std::string, std::unique_ptr<OrderBook>>>();
        auto stockWithAccountsPtr = std::make_shared<std::unordered_map<std::string, std::vector<std::string>>>();

        // 初始化行情服务器连接
        L2TcpSubscriber OrderSubscriber(host, order_port, username, password, "order", orderBooksPtr);
        L2TcpSubscriber TradeSubscriber(host, trade_port, username, password, "trade", orderBooksPtr);

        // 登录行情服务器
        OrderSubscriber.connect();
        TradeSubscriber.connect(); 

        // 初始化交易信号发送服务器
        auto sendServerPtr = std::make_shared<SendServer>("to_python_pipe"); // 因为要被orderbook调用,所以用shared_ptr
        // while (true){
        //     sendServerPtr->send("<000001.SZ#10002000,231312,account3>");
        //     Sleep(1);
        // }

        // Sleep(5000); // 等待管道服务器启动完成
        // sendServerPtr->send("<000001.SZ#10002000,231312,account3>");

        std::string symbol = "000547.SZ";

        (*orderBooksPtr)[symbol] = std::make_unique<OrderBook>(
            symbol, 
            sendServerPtr,
            stockWithAccountsPtr
        );

        OrderSubscriber.subscribe(symbol); // 订阅逐笔委托
        TradeSubscriber.subscribe(symbol); // 订阅逐笔成交
            
            
        // // 前端消息接收服务器回调函数
        // auto handleMessage = [
        //     stockWithAccountsPtr,
        //     orderBooksPtr,
        //     sendServerPtr,
        //     &OrderSubscriber,
        //     &TradeSubscriber
        // ] (const std::string& message) {
        //     std::string symbol = parseAndStoreStockAccount(message, stockWithAccountsPtr);
            
        //     if (symbol.empty()) {
        //         LOG_WARN(module_name, "从前端消息解析出股票代码为空: {}", message);
        //         return;
        //     }

        //     (*orderBooksPtr)[symbol] = std::make_unique<OrderBook>(
        //         symbol, 
        //         sendServerPtr,
        //         stockWithAccountsPtr
        //     );

        //     OrderSubscriber.subscribe(symbol); // 订阅逐笔委托
        //     TradeSubscriber.subscribe(symbol); // 订阅逐笔成交

        // };

        // // 初始化接收前端消息服务器
        // ReceiveServer recvServer("from_nodejs_pipe", handleMessage); 


       
    
        // std::string test_msg = "<000001.SZ#10002000,231312,account3>";
        // parseAndStoreStockAccount(test_msg, stockWithAccountsPtr);
        // LOG_INFO(module_name, "{}", formatStockAccount("000001.SZ", stockWithAccountsPtr));

        // std::string symbol = "002050.SZ";
        
        // OrderBooksPtr->operator[](symbol) = std::make_unique<OrderBook>(
        //     symbol, 
        //     sendServerPtr,
        //     stockWithAccountsPtr
        // );

        

        // if (OrderSubscriber.connect()) {
        //     OrderSubscriber.subscribe(symbol);
        // }

        // if (TradeSubscriber.connect()) {
        //     TradeSubscriber.subscribe(symbol);
        // }

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
        LOG_ERROR(module_name, "异常终止: {}", ex.what());
        return -1;
    }
}