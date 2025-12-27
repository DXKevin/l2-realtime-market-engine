#include "Logger.h"
#include "L2TcpSubscriber.h"
#include "ConfigReader.h"
#include <iostream>

static const char* module_name = "Main";

int main() {
    try{
        SetConsoleOutputCP(CP_UTF8); // 设置控制台为UTF-8编码以支持中文输出，防止exe运行时命令行输出乱码

        init_log_system("logs/app.log");
        ConfigReader config("config.ini");

        std::string host = config.get("server", "host");
        int order_port = config.getInt("server", "order_port");
        int trade_port = config.getInt("server", "trade_port");

        std::string username = config.get("auth", "username");
        std::string password = config.get("auth", "password");

        LOG_INFO(module_name, "配置文件加载完成");

        L2TcpSubscriber OrderSubscriber(host, order_port, username, password);
        L2TcpSubscriber TradeSubscriber(host, trade_port, username, password);

        if (OrderSubscriber.connect()) {
            OrderSubscriber.subscribe("000001.SZ");
        }

        if (TradeSubscriber.connect()) {
            TradeSubscriber.subscribe("000001.SZ");
        }

    } catch (const std::exception& ex) {
        std::cerr << "异常终止: " << ex.what() << std::endl;
        return -1;
    }
}