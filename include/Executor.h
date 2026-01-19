#pragma once
#include <thread>
#include <atomic>
#include <unordered_map>
#include <memory>
#include <vector>

#include "DataRouter.h"
#include "concurrentqueue/blockingconcurrentqueue.h"
#include "L2TcpSubscriber.h"
#include "SendServer.h"
#include "L2HttpDownloader.h"
#include "ReceiveServer.h"
#include "OrderBook.h"
#include "AutoSaveJsonMap.hpp"
#include "AsyncFileWriter.h"

class Executor {
public:
    Executor();
    ~Executor();
    
    void start();
    void stop();

    bool needsReset() const {return reset_requested_by_executor_.load();}
    void requestReset() {reset_requested_by_executor_.store(true);}
private:
    void init();
    void monitorEventLoop();

    bool isLogined();
    void handleMonitorEvent(const std::string& event);

    std::thread monitorEventThread_;
    std::atomic<bool> running_{false};

    std::string http_url_;
    std::string tcp_host_;
    int order_port_;
    int trade_port_;
    std::string username_;
    std::string password_;

    std::unique_ptr<AsyncFileWriter> asyncFileWriter_;

    std::unique_ptr<std::unordered_map<std::string, std::unique_ptr<OrderBook>>> orderBooks_;
    std::unique_ptr<AutoSaveJsonMap<std::string, std::vector<int>>> stockWithAccounts_;
    std::unique_ptr<moodycamel::BlockingConcurrentQueue<std::string>> monitorEventQueue_;

    std::unique_ptr<L2HttpDownloader> downloader_;
    std::unique_ptr<L2TcpSubscriber> orderSubscriber_; 
    std::unique_ptr<L2TcpSubscriber> tradeSubscriber_; 
    std::unique_ptr<SendServer> sendServer_;
    std::unique_ptr<ReceiveServer> recvServer_;
    std::unique_ptr<DataRouter> dataRouter_;

    // 重启标志物
    mutable std::atomic<bool> reset_requested_by_executor_{false};

    const char* module_name_ = "Executor";
};