// OrderBook.h
// 订单簿管理 - 维护L2行情订单簿，处理委托和成交事件
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


/**
 * @class OrderBook
 * @brief L2订单簿管理器
 * 
 * 功能：
 * - 维护买卖盘口的订单簿
 * - 处理逐笔委托和逐笔成交事件
 * - 处理乱序事件（通过待处理队列）
 * - 支持上交所和深交所的数据格式
 * - 线程安全的事件处理
 * 
 * 使用示例：
 * OrderBook book("600376.SH");
 * book.pushEvent(order_event);
 * book.pushEvent(trade_event);
 */
class OrderBook {
public:
    /**
     * @brief 构造函数
     * @param symbol 股票代码
     */
    explicit OrderBook(const std::string& symbol);
    ~OrderBook();

    /**
     * @brief 推送市场事件到处理队列
     * @param event 委托或成交事件
     * 
     * 线程安全，可从多个线程调用
     */
    void pushEvent(const MarketEvent& event);
    
    /**
     * @brief 停止订单簿处理
     */
    void stop();
    
private:
    // 事件处理循环（在独立线程中运行）
    void runProcessingLoop();
    
    // 事件处理函数
    void handleOrderEvent(const MarketEvent& event);
    void handleTradeEvent(const MarketEvent& event);
    void processPendingEvents();

    // 订单簿操作
    bool isOrderExists(const std::string& order_id) const;
    void addOrder(const L2Order& order);
    void onTrade(const L2Trade& trade);
    void removeOrder(const std::string& order_id);
    void printOrderBook(int level_num) const;

    // 订单簿相关数据结构
    struct OrderRef {
        int volume;
        int price;
        int side;
        std::string id;
    };
    
    std::string symbol_;

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
};
