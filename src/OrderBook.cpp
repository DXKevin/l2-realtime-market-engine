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

void OrderBook::pushEvent(const MarketEvent& event) {
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
    int count = 0;
    while (running_) {
        MarketEvent evt;
        event_queue.wait_dequeue(evt);

        if (!running_) break;

        if (evt.type == MarketEvent::EventType::ORDER) {
            handleOrderEvent(evt);

            //每处理10次委托，处理一次待处理事件
            ++count;
            if (count == 10) {
                processPendingEvents();
                count = 0;
            }
            
        } else if (evt.type == MarketEvent::EventType::TRADE) {
            handleTradeEvent(evt);
        }

        printOrderBook(5);
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
                removeOrder(order.id);
                return;
            } else {
                // 未找到订单，加入等待队列
                pending_events_.push_back(event);
                return;
            }

        } else if (order.type == 2)
        {   
            addOrder(order);
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
            addOrder(order);
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
                removeOrder(trade.buy_id);
                return;
            } else {
                // 未找到订单，加入等待队列
                pending_events_.push_back(event);
                return;
            }
        } else {
            if (isOrderExists(trade.sell_id)) {
                removeOrder(trade.sell_id);
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
            onTrade(trade);
        } else if ((isOrderExists(trade.buy_id) || isOrderExists(trade.sell_id))) {
            // 只有一方订单存在于订单簿中, 检查是否有一方为市价单
            if (null_price_order_ids_.find(trade.buy_id) != null_price_order_ids_.end() ||
                null_price_order_ids_.find(trade.sell_id) != null_price_order_ids_.end()) {
                // 因为市价单肯定不存在于订单簿中, 那存在的肯定是正常订单, 可以直接处理成交
                onTrade(trade);

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
    const size_t max_process = 5; // 每次最多处理5个待处理事件
    size_t processed_index = 0;

    while (processed_index < max_process && !pending_events_.empty()) {
        MarketEvent event = std::move(pending_events_.front());
        pending_events_.pop_front();

        if (event.type == MarketEvent::EventType::ORDER) {
            handleOrderEvent(event);
        } else if (event.type == MarketEvent::EventType::TRADE) {
            handleTradeEvent(event);
        }
        ++processed_index;
    }
}

// 检查订单是否存在
bool OrderBook::isOrderExists(const std::string& order_id) const {
    return !order_id.empty() && order_index_.find(order_id) != order_index_.end();
}

// 添加订单到订单簿
void OrderBook::addOrder(const L2Order& order) {
    auto& book = (order.side == 1) ? bids_ : asks_;
    auto& list = book[order.price];

    list.push_back({order.volume, order.price, order.side, order.id});
    order_index_[order.id] = std::prev(list.end());

    auto& volume_map = (order.side == 1) ? bid_volume_at_price_ : ask_volume_at_price_;
    volume_map[order.price] += order.volume;
    order.info();
}

// 处理成交
void OrderBook::onTrade(const L2Trade& trade) {
    auto reduce_volume = [&](const std::string& id) {
        auto index_it = order_index_.find(id);
        if (index_it == order_index_.end()) {
            return;
        }
        // 取成交量和挂单量较小值进行减少, 防止聚合订单簿与总订单簿不一致
        int reduce_fix = std::min(trade.volume, index_it->second->volume);
        index_it->second->volume -= reduce_fix;
        
        int price = index_it->second->price;
        int side = index_it->second->side;
        
        
        // 更新价格档位总挂单量
        if (side == 1) {
            bid_volume_at_price_[price] -= reduce_fix;
            if (bid_volume_at_price_[price] <= 0) {
                bid_volume_at_price_.erase(price);
            }
        } else {
            ask_volume_at_price_[price] -= reduce_fix;
            if (ask_volume_at_price_[price] <= 0) {
                ask_volume_at_price_.erase(price);
            }
        }


        if (index_it->second->volume == 0) {
            // 如果订单量为0, 从订单簿和索引中移除
            removeOrder(id);

        } else if (index_it->second->volume < 0)
        {
            removeOrder(id);
            LOG_WARN(module_name, "[{}] 订单ID:{} 处理后数量为负, 已经移除该订单", 
                symbol_, id);
        }
    };
    

    reduce_volume(trade.buy_id);
    reduce_volume(trade.sell_id);

    trade.info();
}

// 从订单簿中移除订单
void OrderBook::removeOrder(const std::string& order_id) {
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
    int volume = order_iter->volume;

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


void OrderBook::printOrderBook(int level_num) const {
    LOG_INFO(module_name, "===== OrderBook Top {} for {} =====", level_num, symbol_);

    // 卖盘（Asks）：价格从低到高（asks_ 是升序 map）
    LOG_INFO(module_name, "Asks (Sell):");
    int ask_count = 0;
    for (auto it = ask_volume_at_price_.begin(); 
         it != ask_volume_at_price_.end() && ask_count < level_num; 
         ++it) {
        double price = it->first / 10000.0;
        int total_vol = it->second;
        LOG_INFO(module_name, "  {:8.4f}  {:10}", price, total_vol);
        ++ask_count;
    }

    // 买盘（Bids）：价格从高到低（bid_volume_at_price_ 是升序 map，需反向）
    LOG_INFO(module_name, "Bids (Buy):");
    int bid_count = 0;
    for (auto it = bid_volume_at_price_.rbegin(); 
         it != bid_volume_at_price_.rend() && bid_count < level_num; 
         ++it) {
        double price = it->first / 10000.0;
        int total_vol = it->second;
        LOG_INFO(module_name, "  {:8.4f}  {:10}", price, total_vol);
        ++bid_count;
    }

    LOG_INFO(module_name, "==================================");
}



