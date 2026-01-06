#include "OrderBook.h"
#include "Logger.h"
#include <iostream>


static const char* module_name = "OrderBook";
static const int EVENT_TIMEOUT_MS = 20000; // 事件处理超时阈值，单位毫秒

OrderBook::OrderBook(
    const std::string& symbol, 
    std::shared_ptr<SendServer> send_server,
    std::shared_ptr<std::unordered_map<std::string, std::vector<std::string>>> stock_with_accounts
) : 
    symbol_(symbol), send_server_(send_server), stock_with_accounts_(stock_with_accounts) {

    if (symbol_.empty()){
        LOG_ERROR(module_name, "OrderBook 无法用空股票代码初始化~");
        return;
    };

    if (send_server_ == nullptr){
        LOG_ERROR(module_name, "OrderBook 初始化时 SendServer 为空指针~");
        return;
    };

    if (stock_with_accounts_ == nullptr){
        LOG_ERROR(module_name, "OrderBook 初始化时 stock_with_accounts 为空指针~");
        return;
    }

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

    int process_pending_events_count = 0;
    int print_book_count = 0;

    const int PROCESS_PENDING_EVENTS_INTERVAL = 10; // 每处理 10 笔事件处理一次待处理事件
    const int PRINT_INTERVAL_EVENTS = 10; // 每处理 10 笔事件打印一次

    while (running_) {
        MarketEvent evt;
        event_queue.wait_dequeue(evt);

        if (!running_) break;

        if (evt.type == MarketEvent::EventType::ORDER) {
            handleOrderEvent(evt);

            // 定期处理待处理事件
            ++process_pending_events_count;
            if (process_pending_events_count == PROCESS_PENDING_EVENTS_INTERVAL) {
                LOG_INFO(module_name, "等待处理的事件数量: {}", pending_events_.size());
                processPendingEvents();
                process_pending_events_count = 0;
            }
            
        } else if (evt.type == MarketEvent::EventType::TRADE) {
            handleTradeEvent(evt);
        }

        // 检查涨停撤单
        // checkLimitUpWithdrawal();

        // 定期打印订单簿
        ++print_book_count;
        if (print_book_count == PRINT_INTERVAL_EVENTS) {
            printOrderBook(5);
            print_book_count = 0;
        }
    }
}

// 处理逐笔委托
void OrderBook::handleOrderEvent(const MarketEvent& event){

    const auto& order = std::get<L2Order>(event.data);

    if (order.timestamp > last_event_timestamp_) {
        last_event_timestamp_ = order.timestamp;
    }

    if (order.symbol[0] == '6'){
        // 上海逐笔委托处理逻辑
        // 上海逐笔委托 --> 限价2 --> 可以入队
        // 上海逐笔委托 --> 撤单10 --> 要处理乱序的可能
        // 上海逐笔委托 --> 市价单1/本方最优3 --> 这种订单由于价格未知不会加入Book
        if (order.type == 10) {
            // 处理撤单可能存在的乱序情况
            if (isOrderExists(order.id)) {
                onCancelOrder(order.id);
                return;
            } else {
                // 未找到订单，加入等待队列
                if (order.timestamp + EVENT_TIMEOUT_MS < last_event_timestamp_) {
                    LOG_WARN(module_name, "[{}] 长时间未找到订单ID: {} 的撤单请求，可能数据有问题，丢弃该撤单请求", symbol_, order.id);
                    return;
                }

                pending_events_.push_back(event);
                return;
            }

        } else if (order.type == 2)
        {   
            addOrder(order);
        } else {
            // 处理市价与本方最优逐笔委托
            return;
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
            return;
        }
    }
}

// 处理逐笔成交
void OrderBook::handleTradeEvent(const MarketEvent& event){
    const auto& trade = std::get<L2Trade>(event.data);

    if (trade.timestamp > last_event_timestamp_) {
        last_event_timestamp_ = trade.timestamp;
    }

    if (trade.type == 1){
        // 处理深圳撤单逻辑
        // 如果逐笔成交内成交类型为1, 则代表这一定是深圳的撤单字段
        // 处理撤单可能存在的乱序情况

        auto handle_cancel = [this, &event, &trade](const std::string& order_id) {
            if (isOrderExists(order_id)) {
                onCancelOrder(order_id);
                return;
            } else {
                
                if (trade.timestamp + EVENT_TIMEOUT_MS < last_event_timestamp_) {
                    // 长时间未找到订单，丢弃该撤单请求
                    return;
                } else {
                    // 未找到订单，加入等待队列
                    pending_events_.push_back(event);
                    return;
                }
            }
        };

        handle_cancel(trade.buy_id);
        handle_cancel(trade.sell_id);

    } else {
        // 处理成交逻辑
        // 找到buy_id和sell_id对应的订单，减少其数量
        // 处理乱序, 可能找不到订单
        if (isOrderExists(trade.buy_id) && isOrderExists(trade.sell_id)) {
            // 成交双方的订单都存在于订单簿中, 直接处理成交
            onTrade(trade);
        } else if ((isOrderExists(trade.buy_id) || isOrderExists(trade.sell_id))) {
            // 成交双方只有一方存在于订单簿中
            // 另一方订单不存在存在三种情况
            // 1、市价单, 2、乱序到达, 3、临时启动程序无原始盘口, 
            // 优先处理存在的一方并加入等待队列, 等待另一方订单到达
            onTrade(trade);
            pending_events_.push_back(event);
        } else {
            // 双方订单都不存在于订单簿中, 可能是乱序, 加入等待队列
            if (trade.timestamp + EVENT_TIMEOUT_MS < last_event_timestamp_) {
                LOG_WARN(module_name, "[{}] 长时间未找到双方订单ID: 买方订单ID: {}, 卖方订单ID: {} 的成交请求，可能数据有问题，丢弃该成交请求", 
                    symbol_, trade.buy_id, trade.sell_id);
                return;
            } else {
                pending_events_.push_back(event);
            }
        } 
    }
}

// 处理待处理事件队列
void OrderBook::processPendingEvents() {
    // const size_t max_process = 10000; // 每次最多处理5个待处理事件
    const size_t max_process = pending_events_.size(); // 每次处理所有待处理事件
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
    // order.info();
}

// 处理成交
void OrderBook::onTrade(const L2Trade& trade) {

    auto reduce_volume = [&](const std::string& id) {
        auto index_it = order_index_.find(id);
        if (index_it == order_index_.end()) {
            return;
        }
        // 取成交量和挂单量较小值进行减少, 防止聚合订单簿与总订单簿不一致
        // int reduce_fix = std::min(trade.volume, index_it->second->volume);

        index_it->second->volume -= trade.volume;
        
        int price = index_it->second->price;
        int side = index_it->second->side;
        
        // 更新价格档位总挂单量
        if (side == 1) {
            bid_volume_at_price_[price] -= trade.volume;
            if (bid_volume_at_price_[price] <= 0) {
                bid_volume_at_price_.erase(price);
            }
        } else {
            ask_volume_at_price_[price] -= trade.volume;
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
            // LOG_WARN(module_name, "[{}] 订单ID:{} 处理后数量为负, 已经移除该订单", 
            //     symbol_, id);
        }
    };
    

    reduce_volume(trade.buy_id);
    reduce_volume(trade.sell_id);

    // trade.info();
}

// 处理撤单
void OrderBook::onCancelOrder(const std::string& order_id) {
    auto index_it = order_index_.find(order_id);
    if (index_it == order_index_.end()) {
        return;
    }
    
    int price = index_it->second->price;
    int side = index_it->second->side;
    int volume = index_it->second->volume;
    
    // 更新价格档位总挂单量
    if (side == 1) {
        bid_volume_at_price_[price] -= volume;
        if (bid_volume_at_price_[price] <= 0) {
            bid_volume_at_price_.erase(price);
        }
    } else {
        ask_volume_at_price_[price] -= volume;
        if (ask_volume_at_price_[price] <= 0) {
            ask_volume_at_price_.erase(price);
        }
    }

    removeOrder(order_id);

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

        // LOG_INFO(module_name, "[{}] Remove order success: id={}", symbol_, order_id);
        return; 
    }
}

// 打印订单簿前 N 档
void OrderBook::printOrderBook(int level_num) const {

    LOG_INFO(module_name, "===== OrderBook Top {} for {} =====", level_num, symbol_);

    // 卖盘（Asks）：价格从低到高（asks_ 是升序 map）
    LOG_INFO(module_name, "Asks (Sell):");

    std::vector<std::pair<int, int>> ask_levels;

    int ask_count = 0;
    for (auto it = ask_volume_at_price_.begin(); 
         it != ask_volume_at_price_.end() && ask_count < level_num; 
         ++it, ++ask_count) {
            
            ask_levels.emplace_back(it->first, it->second);
    }

    for (auto it = ask_levels.rbegin(); it != ask_levels.rend(); ++it) {

        double price = it->first / 10000.0;
        int total_vol = it->second;
        LOG_INFO(module_name, "  {:8.4f}  {:10}", price, total_vol);
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

// 检查涨停撤单情况
void OrderBook::checkLimitUpWithdrawal() {

    // 如果没有买盘则直接返回
    if (bid_volume_at_price_.empty()) {
        return;
    }

    auto best_bid_it = bid_volume_at_price_.rbegin();
    int current_volume  = best_bid_it->second;
    int current_price  = best_bid_it->first;

    // 更新最大买一量
    if (current_volume > max_bid_volume_){
        max_bid_volume_ = current_volume;
        // LOG_INFO(module_name, "[{}] 创历史最高买一量: {}, 价格: {}", symbol_, max_bid_volume_, current_price / 10000.0);
        
        return; // 若发生更新, 肯定是买盘增加了, 直接返回
    }

    // 检查是否触发涨停撤单监测条件
    if (max_bid_volume_ > 0 && current_volume * 3 < max_bid_volume_ * 2) {
        
        // 放发生交易请求的部分
        if (send_server_) {
            send_server_->send("Limit up withdrawal detected for " + symbol_);
        }

        LOG_WARN(module_name, "[{}] 涨停撤单警告: 当前买一量 {} 低于历史最高买一量 {} 的 2/3", 
            symbol_, current_volume, max_bid_volume_);
    }
}

