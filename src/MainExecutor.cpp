#include "MainExecutor.h"
#include "FileOperator.h"
#include "Logger.h"
#include "L2Parser.h"

#include <vector>

static const char * module_name = "MainExecutor";
static const char * monitor_event_filename = "monitor_event.txt";

MainExecutor::MainExecutor(
    moodycamel::BlockingConcurrentQueue<std::string>& monitorEventQueue,
    std::unordered_map<std::string, std::unique_ptr<OrderBook>>& orderBooks,
    std::unordered_map<std::string, std::vector<std::string>>& stockWithAccounts,
    SendServer& sendServer,
    L2TcpSubscriber& orderSubscriber,
    L2TcpSubscriber& tradeSubscriber,
    L2HttpDownloader& downloader
    ): 
    monitorEventQueue_(monitorEventQueue), 
    orderBooks_(orderBooks), 
    stockWithAccounts_(stockWithAccounts), 
    sendServer_(sendServer), 
    orderSubscriber_(orderSubscriber), 
    tradeSubscriber_(tradeSubscriber), 
    downloader_(downloader) {
    
    monitorEventThread_ = std::thread(&MainExecutor::run, this);
}

MainExecutor::~MainExecutor() {
    if (monitorEventThread_.joinable()) {
        monitorEventThread_.join();
    }
}   

bool MainExecutor::isLogined() {
    return orderSubscriber_.is_logined_ &&
         tradeSubscriber_.is_logined_ &&
          downloader_.is_logined_;
}

void MainExecutor::run() {
    while (true) {
        if (!isLogined()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        // 读取本地文件中的监控事件
        std::vector<std::string> events = readTxtFile(monitor_event_filename);
        for (const auto& event : events) {
            // 处理监控事件
            handleMonitorEvent(event);
        }

        while (isLogined()) {
            std::string event;
            monitorEventQueue_.wait_dequeue(event);
            
            // 将从前端接受`的监控事件写入本地文件
            writeTxtFile(monitor_event_filename, event);   
            
            // 处理监控事件
            handleMonitorEvent(event);
        }
    }
}

void MainExecutor::handleMonitorEvent(const std::string& event) {
    std::string symbol = parseAndStoreStockAccount(event, stockWithAccounts_);

    if (symbol.empty()) {
        LOG_WARN(module_name, "从前端消息解析出股票代码为空: {}", event);
        return;
    }

    if (orderBooks_.find(symbol) != orderBooks_.end()) {
        LOG_INFO(module_name, "股票代码 {} 的 OrderBook 已存在，跳过创建新实例", symbol);
        return;
    }

    orderBooks_[symbol] = std::make_unique<OrderBook>(
        symbol, 
        sendServer_,
        stockWithAccounts_
    );

    // 订阅逐笔委托和逐笔成交
    orderSubscriber_.subscribe(symbol); 
    tradeSubscriber_.subscribe(symbol); 

    // 等待10秒
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // 下载历史数据
    downloader_.start_download_async(symbol, "Order"); 
    downloader_.start_download_async(symbol, "Tran");
}
    