#pragma once
#include <string>
#include <map>
#include <list>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <deque>
#include <set>
#include <functional>
#include <unordered_set>

#include "concurrentqueue/blockingconcurrentqueue.h"
#include "DataStruct.h"
#include "SendServer.h"


class OrderBook {
public:
    explicit OrderBook(
        const std::string symbol,
        std::shared_ptr<SendServer> send_server,
        std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> stock_with_accounts
    );
    ~OrderBook();

    void pushHistoryEvent(const MarketEvent& event);
    void pushEvent(const MarketEvent& event);
    void stop();

    std::atomic<bool> is_history_done_{false};
private:
    void runProcessingLoop();
    void handleOrderEvent(const MarketEvent& event);
    void handleTradeEvent(const MarketEvent& event);
    void processPendingEvents();

    bool isOrderExists(const std::string& order_id) const;
    void addOrder(const L2Order& order);
    void onTrade(const L2Trade& trade);
    void onCancelOrder(const std::string& order_id, int cancel_volume);
    void removeOrder(const std::string& order_id);
    void printOrderBook(int level_num) const;
    void printloop(int level_num);

    void checkLimitUpWithdrawal();

    // 订单簿相关数据结构
    struct OrderRef {
        int volume;
        int price;
        int side;
        std::string id;
    };
    
    std::string symbol_;
    
    int max_bid_volume_ = 0; // 最大封单量
    int last_event_timestamp_ = 0; // 最后一笔事件的时间戳

    std::vector<std::pair<int, int>> history_order_timeId;
    std::vector<std::pair<int, int>> history_trade_timeId;
    std::unordered_set<int> history_order_id;
    std::unordered_set<int> history_trade_id;

    // 封单比例时间窗口
    std::map<int, double> limit_up_fengdan_ratios_;
    // 封单比例有序集合
    std::multiset<double> fengdan_ratio_set_;
    
    // 价格 → 该档位总挂单量
    std::map<int, int> bid_volume_at_price_;
    std::map<int, int> ask_volume_at_price_;

    // 按价格分组的买卖订单簿
    std::map<int,std::list<OrderRef>> bids_;
    std::map<int,std::list<OrderRef>> asks_;

    // 暂存待处理事件
    std::deque<MarketEvent> pending_events_; 

    // 快速查找订单
    std::unordered_map<std::string, std::list<OrderRef>::iterator> order_index_;

    // 事件队列 - MPSC
    moodycamel::BlockingConcurrentQueue<MarketEvent> history_event_queue;
    moodycamel::BlockingConcurrentQueue<MarketEvent> event_queue;

    // 工作线程
    std::thread processing_thread_;
    std::thread print_thread_;
    std::atomic<bool> running_{true};
    std::atomic<bool> is_send_{false};
    
    // 锁
    mutable std::mutex mtx_;

    // 外部关联数据
    std::shared_ptr<SendServer> send_server_;
    std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> stock_with_accounts_;
};
