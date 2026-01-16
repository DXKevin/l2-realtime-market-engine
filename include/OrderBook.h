#pragma once
#include <string>
#include <map>
#include <list>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <deque>
#include <unordered_set>

#include "concurrentqueue/blockingconcurrentqueue.h"
#include "DataStruct.h"
#include "SendServer.h"


class OrderBook {
public:
    explicit OrderBook(
        const std::string symbol,
        SendServer& sendServer_ref,
        std::unordered_map<std::string, std::vector<std::string>>& stockWithAccounts_ref
    );
    ~OrderBook();

    void pushHistoryEvent(const MarketEvent& event);
    void pushEvent(const MarketEvent& event);
    void stop();

    std::atomic<bool> is_history_order_done_{false};
    std::atomic<bool> is_history_trade_done_{false};

private:
    bool isHistoryDataLoadingComplete() const;
    void generateDuplicateSets();
    void runProcessingLoop();
    void handleOrderEvent(const MarketEvent& event);
    void handleTradeEvent(const MarketEvent& event);
    void processPendingEvents();

    bool isBuyOrderDone(const int num1) const;
    bool isSellOrderDone(const int num1) const;
    bool isOrderExists(const int order_id) const;
    void addOrder(const L2Order& order);
    void onTrade(const int order_id, const int trade_volume, const int trade_side);
    void onCancelOrder(const int order_id, const int cancel_volume);
    void removeOrder(const int order_id);
    void printOrderBook(int level_num) const;
    void printloop(int level_num);

    void checkLimitUpWithdrawal(int timestamp);

    // 订单簿相关数据结构
    struct OrderRef {
        int volume;
        int price;
        int side;
        int id;
    };
    
    std::string symbol_;
    
    int max_bid_volume_ = 0; // 最大封单量
    int last_event_timestamp_ = 0; // 最后一笔事件的时间戳

    int EVENT_TIMEOUT_MS = 60000; // 事件处理超时阈值，单位毫秒

    // 历史事件排序缓冲区
    std::vector<MarketEvent> history_event_buffer_;

    // 历史数据去重集合
    std::vector<std::pair<int, int>> history_order_timeId_;
    std::vector<std::pair<int, int>> history_trade_timeId_;
    std::unordered_set<int> history_order_id_;
    std::unordered_set<int> history_trade_id_;

    // 封单比例时间窗口
    std::map<int, double> limit_up_fengdan_ratios_;
    
    // 价格 → 该档位总挂单量
    std::map<int, int> bid_volume_at_price_;
    std::map<int, int> ask_volume_at_price_;

    // 按价格分组的买卖订单簿
    std::map<int,std::list<OrderRef>> bids_;
    std::map<int,std::list<OrderRef>> asks_;

    // 已完成买单对应成交编号集合, 对于逐笔成交中只有一方存在加入等待列表会出现重复成交扣减的情况处理
    std::unordered_set<int> buy_order_done_ids_; 
    std::unordered_set<int> sell_order_done_ids_;

    // 暂存待处理事件
    std::deque<MarketEvent> pending_events_; 

    // 快速查找订单
    std::unordered_map<int, std::list<OrderRef>::iterator> order_index_;

    // 事件队列 - MPSC
    moodycamel::BlockingConcurrentQueue<MarketEvent> history_event_queue;
    moodycamel::BlockingConcurrentQueue<MarketEvent> event_queue;

    // 工作线程
    std::thread processing_thread_;
    std::thread print_thread_;
    std::atomic<bool> running_{true};
    std::atomic<bool> is_send_{false};
    std::atomic<bool> is_history_event_queue_done_{false};
    std::atomic<bool> is_history_event_buffer_done_{false};
    
    // 锁
    mutable std::mutex mtx_;

    // 外部关联数据
    SendServer& sendServer_ref_;
    std::unordered_map<std::string, std::vector<std::string>>& stockWithAccounts_ref_;
};
