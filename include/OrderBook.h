#pragma once
#include <string>
#include <map>
#include <list>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <deque>
#include <functional>
#include <unordered_set>

#include "concurrentqueue/blockingconcurrentqueue.h"
#include "DataStruct.h"
#include "SendServer.h"


class OrderBook {
public:
    explicit OrderBook(
        const std::string& symbol,
        std::shared_ptr<SendServer> send_server,
        std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> stock_with_accounts
    );
    ~OrderBook();

    void pushEvent(const MarketEvent& event);
    void stop();
private:
    void runProcessingLoop();
    void handleOrderEvent(const MarketEvent& event);
    void handleTradeEvent(const MarketEvent& event);
    void processPendingEvents();

    bool isOrderExists(const std::string& order_id) const;
    void addOrder(const L2Order& order);
    void onTrade(const L2Trade& trade);
    void removeOrder(const std::string& order_id);
    void printOrderBook(int level_num) const;

    void checkLimitUpWithdrawal();

    // 订单簿相关数据结构
    struct OrderRef {
        int volume;
        int price;
        int side;
        std::string id;
    };
    
    std::string symbol_;
    
    int max_bid_volume_ = 0; // 历史最高买一量

    // 价格 → 该档位总挂单量
    std::map<int, int> bid_volume_at_price_;
    std::map<int, int> ask_volume_at_price_;

    // 按价格分组的买卖订单簿
    std::map<int,std::list<OrderRef>> bids_;
    std::map<int,std::list<OrderRef>> asks_;

    // 暂存待处理事件
    std::deque<MarketEvent> pending_events_; 

    // 暂存市价单ID
    std::unordered_set<std::string> null_price_order_ids_;

    // 快速查找订单
    std::unordered_map<std::string, std::list<OrderRef>::iterator> order_index_;

    // 事件队列 - MPSC
    moodycamel::BlockingConcurrentQueue<MarketEvent> event_queue;
    
    // 工作线程
    std::thread processing_thread_;
    std::atomic<bool> running_{true};

    // 外部关联数据
    std::shared_ptr<SendServer> send_server_;
    std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> stock_with_accounts_;
};
