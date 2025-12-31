#include "OrderBook.h"
#include "logger.h"
#include <iostream>


static const char* module_name = "OrderBook";


OrderBook::OrderBook(const std::string& symbol) : symbol_(symbol) {
    if (symbol_.empty()){
        LOG_ERROR(module_name, "OrderBook 无法用空股票代码初始化~");
        return;
    };
    processing_thread_ = std::thread(&OrderBook::runProcessingLoop, this);
}

OrderBook::~OrderBook() {
    stop();
}

void OrderBook::pushEvent(const MarketEvent event) {
    event_queue.enqueue(event);
}

void OrderBook::stop() {
    // exchange 设置 running_ 为 false, 并返回之前的值
    if (running_.exchange(false)) {
        event_queue.enqueue(MarketEvent{}); // 触发退出
        if (processing_thread_.joinable()) {
            processing_thread_.join();
        }
    }
}

// 主线程循环
void OrderBook::runProcessingLoop() {
    while (running_) {
        MarketEvent evt;
        event_queue.wait_dequeue(evt);

        if (!running_) break;

        if (evt.type == MarketEvent::EventType::ORDER) {
            handleOrderEvent(evt);

            // 有新的委托处理后，尝试处理待处理队列
            processPendingEvents();
        } else if (evt.type == MarketEvent::EventType::TRADE) {
            handleTradeEvent(evt);
        }

        //print_top5();
    }
}

// 处理逐笔委托
void OrderBook::handleOrderEvent(const MarketEvent& event){

    const auto& order = std::get<L2Order>(event.data);

    if (order.symbol[0] == '6'){
        // 上海逐笔委托处理逻辑
        //上海逐笔委托 --> 限价2 --> 可以入队
        // 上海逐笔委托 --> 撤单10 --> 要处理乱序的可能
        /* 上海逐笔委托 --> 市价单1/本方最优3 --> 这种订单由于价格未知不会加入Book, 
            但是会存下id, 用于后续处理trade, 虽然很少是这类单子但是也要处理 */
        if (order.type == 10) {
            // 处理撤单可能存在的乱序情况
            if (isOrderExists(order.id)) {
                remove_order(order.id);
                return;
            } else {
                // 未找到订单，加入等待队列
                pending_events_.push_back(event);
                return;
            }

        } else if (order.type == 2)
        {   
            add_order(order);
        } else {
            // 处理市价与本方最优逐笔委托
            null_price_order_ids_.insert(order.id);
        }
        
    } else {
        // 深圳逐笔委托处理逻辑
        // 深圳逐笔委托 --> 限价2 --> 可以入队
        /* 深圳逐笔委托 --> 市价单1/本方最优3 --> 这种订单由于价格未知不会加入Book, 
            但是会存下id, 用于后续处理trade, 虽然很少是这类单子但是也要处理 */
        if (order.type == 2) {
            add_order(order);
        } else {
            // 处理市价与本方最优逐笔委托
            null_price_order_ids_.insert(order.id);
        }
    }
}

// 处理逐笔成交
void OrderBook::handleTradeEvent(const MarketEvent& event){
    const auto& trade = std::get<L2Trade>(event.data);

    if (trade.type == 1){
        // 如果逐笔成交内成交类型为1, 则代表这一定是深圳的撤单字段
        // 处理撤单可能存在的乱序情况
        if (trade.side == 1) {
            if (isOrderExists(trade.buy_id)) {
                remove_order(trade.buy_id);
                return;
            } else {
                // 未找到订单，加入等待队列
                pending_events_.push_back(event);
                return;
            }
        } else {
            if (isOrderExists(trade.sell_id)) {
                remove_order(trade.sell_id);
                return;
            } else {
                // 未找到订单，加入等待队列
                pending_events_.push_back(event);
                return;
            }
        }
    } else {
        // 成交处理逻辑
        // 找到buy_id和sell_id对应的订单，减少其数量
        // 处理乱序, 可能找不到订单
        if (isOrderExists(trade.buy_id) && isOrderExists(trade.sell_id)) {
            // 成交双方的订单都存在于订单簿中, 直接处理成交
            on_trade(trade);
        } else if ((isOrderExists(trade.buy_id) || isOrderExists(trade.sell_id))) {
            // 只有一方订单存在于订单簿中, 检查是否有一方为市价单
            if (null_price_order_ids_.find(trade.buy_id) != null_price_order_ids_.end() ||
                null_price_order_ids_.find(trade.sell_id) != null_price_order_ids_.end()) {
                // 因为市价单肯定不存在于订单簿中, 那存在的肯定是正常订单, 可以直接处理成交
                on_trade(trade);

                // 移除市价单ID
                null_price_order_ids_.erase(trade.buy_id);
                null_price_order_ids_.erase(trade.sell_id);
            } else {
                // 另一方订单不存在且不是市价单, 可能是乱序, 加入等待队列
                pending_events_.push_back(event);
                return;
            }
        } else {
            // 双方订单都不存在于订单簿中, 可能是乱序, 加入等待队列
            pending_events_.push_back(event);
            return;
        } 
    }
}

void OrderBook::processPendingEvents() {
    size_t count = pending_events_.size();
    for (size_t i = 0; i < count; ++i) {
        MarketEvent event = pending_events_.front();
        pending_events_.pop_front();

        if (event.type == MarketEvent::EventType::ORDER) {
            handleOrderEvent(event);
        } else if (event.type == MarketEvent::EventType::TRADE) {
            handleTradeEvent(event);
        }
    }
}

// 检查订单是否存在
bool OrderBook::isOrderExists(const std::string& order_id) const {
    return !order_id.empty() && order_index_.find(order_id) != order_index_.end();
}

// 添加订单到订单簿
void OrderBook::add_order(const L2Order& order) {
    auto& book = (order.side == 1) ? bids_ : asks_;
    auto& list = book[order.price];

    list.push_back({order.volume, order.price, order.side, order.id});
    order_index_[order.id] = std::prev(list.end());

    order.info();
}

// 处理成交
void OrderBook::on_trade(const L2Trade& trade) {
    auto id_it = order_index_.find(trade.sell_id);

    if (id_it != order_index_.end()) {
        id_it->second->volume -= trade.volume;

        if (id_it->second->volume == 0) {
            // 如果订单量为0, 从订单簿和索引中移除
            remove_order(trade.sell_id);
        } else if (id_it->second->volume < 0)
        {
            remove_order(trade.sell_id);
            LOG_WARN(module_name, "[{}] 订单ID:{} 处理后数量为负, 已经移除该订单", 
                symbol_, trade.sell_id);
        }
    }

    id_it = order_index_.find(trade.buy_id);
    if (id_it != order_index_.end()) {
        id_it->second->volume -= trade.volume;

        if (id_it->second->volume == 0) {
            // 如果订单量为0, 从订单簿和索引中移除
            remove_order(trade.buy_id);
        } else if (id_it->second->volume < 0)
        {
            remove_order(trade.buy_id);
            LOG_WARN(module_name, "[{}] 订单ID:{} 处理后数量为负, 已经移除该订单", 
                symbol_, trade.buy_id);
        }
    }

    trade.info();
}

// 从订单簿中移除订单
void OrderBook::remove_order(const std::string& order_id) {
    // 查找订单, 返回键值对迭代器
    auto index_it = order_index_.find(order_id);
    if (index_it == order_index_.end()) {
        // 未找到订单
        return;
    }

    // 获取订单引用指针
    auto order_iter = index_it->second;

    // 取出订单信息
    int price = order_iter->price;
    int side = order_iter->side;

    // 拿到买方盘口或者卖方盘口
    auto& book = (side == 1) ? bids_ : asks_;

    // 拿到对应价格档位键值对迭代器
    auto price_it = book.find(price);
    if (price_it == book.end()) {
        // 未找到价格档位，数据异常
        return;
    } else {
        auto& price_level_list = price_it->second;

        // 从价格档位列表中移除订单,从索引中移除订单
        price_level_list.erase(order_iter);
        order_index_.erase(index_it);
        
        // 如果该价格档位没有订单了, 从订单簿中移除该价格档位
        if (price_level_list.empty()) {
            book.erase(price_it);
        }

        LOG_INFO(module_name, "[{}] Remove order success: id={}", symbol_, order_id);
        return;
    }
}


// 打印买卖盘前5档
void OrderBook::print_top5() const {
    LOG_INFO(module_name, "===== OrderBook Top 5 for {} =====", symbol_);

    // 打印卖盘（asks）—— 价格从低到高，取前5档
    LOG_INFO(module_name, "Asks (Sell):");
    int ask_count = 0;
    for (auto it = asks_.rbegin(); it != asks_.rend() && ask_count < 5; ++it) {
        int total_vol = 0;
        for (const auto& order : it->second) {
            total_vol += order.volume;
        }
        double price = it->first / 10000.0; // 假设 price 是以 0.0001 为单位存储的整数
        LOG_INFO(module_name, "  {:>8.4f}  {:>10}", price, total_vol);
        ++ask_count;
    }

    // 打印买盘（bids）—— 价格从高到低，取前5档
    LOG_INFO(module_name, "Bids (Buy):");
    int bid_count = 0;
    for (auto it = bids_.rbegin(); it != bids_.rend() && bid_count < 5; ++it) {
        int total_vol = 0;
        for (const auto& order : it->second) {
            total_vol += order.volume;
        }
        double price = it->first / 10000.0;
        LOG_INFO(module_name, "  {:>8.4f}  {:>10}", price, total_vol);
        ++bid_count;
    }

    LOG_INFO(module_name, "==================================");
}



