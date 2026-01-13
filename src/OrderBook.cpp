#include "OrderBook.h"
#include "Logger.h"
#include "L2Parser.h"

#include <vector>
#include <iostream>


static const char* module_name = "OrderBook";

OrderBook::OrderBook(
    const std::string symbol, 
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
    print_thread_ = std::thread(&OrderBook::printloop, this, 5);
}

OrderBook::~OrderBook() {
    stop();
}

void OrderBook::pushHistoryEvent(const MarketEvent& event) {
    history_event_queue.enqueue(event);
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

        if (print_thread_.joinable()) {
            print_thread_.join();
        }
    }
}

// 检查历史数据加载是否完成
bool OrderBook::isHistoryDataLoadingComplete() const {
    return (is_history_order_done_.load() && is_history_trade_done_.load());
}

// 生成历史数据去重集合
void OrderBook::generateDuplicateSets() {
    // 生成历史数据去重集合

    for (auto it = history_order_timeId_.begin(); it != history_order_timeId_.end(); ++it) {
        if (last_event_timestamp_ - it->second <= 600000) {
            // 记录一分钟内的历史数据ID
            history_order_id_.insert(it->first);
        }
    }

    // 清空vector释放内存
    std::vector<std::pair<int, int>>().swap(history_order_timeId_);

    for (auto it = history_trade_timeId_.begin(); it != history_trade_timeId_.end(); ++it) {
        if (last_event_timestamp_ - it->second <= 600000) {
            // 记录一分钟内的历史数据ID
            history_trade_id_.insert(it->first);
        }
    }

    // 清空vector释放内存
    std::vector<std::pair<int, int>>().swap(history_trade_timeId_);
}

// 主线程循环
void OrderBook::runProcessingLoop() {

    int process_pending_events_count = 0;

    const int PROCESS_PENDING_EVENTS_INTERVAL = 10; // 每处理 10 笔事件处理一次待处理事件

    while (running_) {
        if (isHistoryDataLoadingComplete() == false ||
            is_history_event_queue_done_.load() == false
        ) {
            // 先处理历史事件队列
            MarketEvent hist_evt;
            if (!history_event_queue.wait_dequeue_timed(hist_evt, std::chrono::milliseconds(3000))) {
                if (isHistoryDataLoadingComplete()) {
                    // 历史数据处理完成, 生成去重集合
                    generateDuplicateSets();
                    is_history_event_queue_done_.store(true);
                    
                    EVENT_TIMEOUT_MS = 60000; // 将事件处理超时阈值设为1分钟
                }
                continue;
            }
            
            std::lock_guard<std::mutex> lock(mtx_);

            if (!running_) break;

            int timestamp = 0;
            if (hist_evt.type == MarketEvent::EventType::ORDER) {
                handleOrderEvent(hist_evt);

                // 记录历史数据时间戳与ID映射
                history_order_timeId_.push_back(
                    {std::get<L2Order>(hist_evt.data).num1, std::get<L2Order>(hist_evt.data).timestamp}
                );

                timestamp = std::get<L2Order>(hist_evt.data).timestamp;
            } else if (hist_evt.type == MarketEvent::EventType::TRADE) {
                handleTradeEvent(hist_evt);

                // 记录历史数据时间戳与ID映射
                history_trade_timeId_.push_back(
                    {std::get<L2Trade>(hist_evt.data).num1, std::get<L2Trade>(hist_evt.data).timestamp}
                );

                timestamp = std::get<L2Trade>(hist_evt.data).timestamp;
            }

            // 定期处理待处理事件
            ++process_pending_events_count;
            if (process_pending_events_count == PROCESS_PENDING_EVENTS_INTERVAL) {
                processPendingEvents();
                process_pending_events_count = 0;
            }
        } else {
            // 处理实时事件队列
            MarketEvent evt;
            event_queue.wait_dequeue(evt);

            std::lock_guard<std::mutex> lock(mtx_);

            if (!running_) break;

            int timestamp = 0;
            if (evt.type == MarketEvent::EventType::ORDER) {
                handleOrderEvent(evt);
                timestamp = std::get<L2Order>(evt.data).timestamp;
            } else if (evt.type == MarketEvent::EventType::TRADE) {
                handleTradeEvent(evt);
                timestamp = std::get<L2Trade>(evt.data).timestamp;
            }

            // 检查涨停撤单
            checkLimitUpWithdrawal(timestamp);

            // 定期处理待处理事件
            ++process_pending_events_count;
            if (process_pending_events_count == PROCESS_PENDING_EVENTS_INTERVAL) {
                processPendingEvents();
                process_pending_events_count = 0;
            }

        }
    }
}

// 处理逐笔委托
void OrderBook::handleOrderEvent(const MarketEvent& event){

    const auto& order = std::get<L2Order>(event.data);

    if (history_order_id_.find(order.num1) != history_order_id_.end()) {
        LOG_INFO(module_name, "出现重复单");
        return;
    }

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
                onCancelOrder(order.id, order.volume);
                return;
            } else {
                // 未找到订单，加入等待队列
                if (order.timestamp + EVENT_TIMEOUT_MS < last_event_timestamp_) {
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

    if (history_trade_id_.find(trade.num1) != history_trade_id_.end()) {
        LOG_INFO(module_name, "出现重复单");
        return;
    }

    if (trade.timestamp > last_event_timestamp_) {
        last_event_timestamp_ = trade.timestamp;
    }

    if (trade.type == 1){
        // 处理深圳撤单逻辑
        // 如果逐笔成交内成交类型为1, 则代表这一定是深圳的撤单字段
        // 处理撤单可能存在的乱序情况

        auto handle_cancel = [&](const int order_id) {
            if (order_id == 0) {
                return;
            }
            
            if (isOrderExists(order_id)) {
                onCancelOrder(order_id, trade.volume);
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
        bool is_exist_buy = isOrderExists(trade.buy_id);
        bool is_exist_sell = isOrderExists(trade.sell_id);


        if (is_exist_buy && is_exist_sell) {
            // 成交双方的订单都存在于订单簿中, 直接处理成交
            onTrade(trade);

            // LOG_INFO(module_name, "[{}] 双方订单均存在于订单簿中,买方ID:{}, 卖方ID:{}", symbol_, trade.buy_id, trade.sell_id);
        } else if ((is_exist_buy || is_exist_sell)) {
            // 成交双方只有一方存在于订单簿中
            // 另一方订单不存在存在三种情况
            // 1、市价单, 2、乱序到达, 3、临时启动程序无原始盘口, 
            // 优先处理存在的一方并加入等待队列, 等待另一方订单到达
            if (symbol_[0] == '6') {
                // 上海市场成交只存在一方在订单簿, 所以处理后不需要加入等待列表
                onTrade(trade);
                return;
            } else {
                // 深圳市场双方都有可能存在于订单簿, 处理一方后还需要等待另一方到达
                if (is_exist_buy) {
                    if (buy_order_done_ids_.find(trade.num1) != buy_order_done_ids_.end()) {
                        // 已处理过该买单对应的成交编号，避免重复处理
                        pending_events_.push_back(event);
                        return;
                    } else {
                        onTrade(trade);
                        pending_events_.push_back(event);
                        buy_order_done_ids_.insert(trade.num1);
                    }
                } else {
                    if (sell_order_done_ids_.find(trade.num1) != sell_order_done_ids_.end()) {
                        // 已处理过该卖单对应的成交编号，避免重复处理
                        pending_events_.push_back(event);
                        return;
                    } else {
                        onTrade(trade);
                        pending_events_.push_back(event);
                        sell_order_done_ids_.insert(trade.num1);
                    }
                }
            }
        } else {
            // 双方订单都不存在于订单簿中, 可能是乱序, 加入等待队列
            if (trade.timestamp + EVENT_TIMEOUT_MS < last_event_timestamp_) {
                // 长时间未找到订单，丢弃该成交请求
                buy_order_done_ids_.erase(trade.num1);
                sell_order_done_ids_.erase(trade.num1);
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
bool OrderBook::isOrderExists(const int order_id) const {
    return order_index_.find(order_id) != order_index_.end();
}

// 添加订单到订单簿
void OrderBook::addOrder(const L2Order& order) {

    auto& book = (order.side == 1) ? bids_ : asks_;
    auto& list = book[order.price];

    list.push_back({order.volume, order.price, order.side, order.id});
    order_index_[order.id] = std::prev(list.end());

    auto& volume_map = (order.side == 1) ? bid_volume_at_price_ : ask_volume_at_price_;
    volume_map[order.price] += order.volume;
    //order.info();
}

// 处理成交
void OrderBook::onTrade(const L2Trade& trade) {

    auto reduce_volume = [&](const int id) {
        auto index_it = order_index_.find(id);
        if (index_it == order_index_.end()) {
            return;
        }

        int price = index_it->second->price;
        int side = index_it->second->side;
        
        if (symbol_[0] == '6' && trade.side == side) {
            // 上海市场买卖方向与成交方向相同的订单不处理封单量, 因为主动成交的一方委托量不会出现在订单簿中
            // if (id == "24529530") {
            //     LOG_INFO(module_name, "主动成交订单ID: {}，价格: {}, 方向: {}", id, price, side);
            // }
            return; 
        } 

        // 更新订单簿
        index_it->second->volume -= trade.volume;

        // if (id == "24529530") {
        //         LOG_INFO(module_name, "被动成交订单ID: {}, 成交数量:{}, 剩余数量: {}, 方向: {}", id, trade.volume, index_it->second->volume, side);
        // }

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

        if (index_it->second->volume <= 0) {
            // 如果订单量为0, 从订单簿和索引中移除
            removeOrder(id);
        }
    };
    

    reduce_volume(trade.buy_id);
    reduce_volume(trade.sell_id);

    // trade.info();
}

// 处理撤单
void OrderBook::onCancelOrder(const int order_id, const int cancel_volume) {
    auto index_it = order_index_.find(order_id);
    if (index_it == order_index_.end()) {
        return;
    }
    
    // 更新订单簿
    index_it->second->volume -= cancel_volume;

    int price = index_it->second->price;
    int side = index_it->second->side;

    // 更新价格档位总挂单量
    if (side == 1) {
        bid_volume_at_price_[price] -= cancel_volume;
        if (bid_volume_at_price_[price] <= 0) {
            bid_volume_at_price_.erase(price);
        }
    } else {
        ask_volume_at_price_[price] -= cancel_volume;
        if (ask_volume_at_price_[price] <= 0) {
            ask_volume_at_price_.erase(price);
        }
    }

    if (index_it->second->volume <= 0) {
        // 如果订单量为0, 从订单簿和索引中移除
        removeOrder(order_id);
    }
}

// 从订单簿中移除订单
void OrderBook::removeOrder(const int order_id) {

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

    int total_bid_list_size = 0;
    for (const auto& [price, order_list] : bids_) {
        
        total_bid_list_size += order_list.size();
    }

    int total_ask_list_size = 0;
    for (const auto& [price, order_list] : asks_) {
        total_ask_list_size += order_list.size();
    }


    // if (last_event_timestamp_ > 53999000) {
    //     for (auto it = asks_.begin(); it != asks_.end(); ++it) {
    //         for (const auto& order_ref : it->second) {
    //             LOG_INFO(module_name, "卖盘订单 - ID: {}, 价格: {}, 数量: {}, 方向: {}", 
    //                 order_ref.id, order_ref.price / 10000.0, order_ref.volume, order_ref.side);
    //         }
    //     }
    // }


    LOG_INFO(module_name, "买盘价格订单总数量 {}", total_bid_list_size);
    LOG_INFO(module_name, "卖盘价格订单总数量 {}", total_ask_list_size);

    LOG_INFO(module_name, "历史事件去重集合大小: {}", history_order_id_.size());
    LOG_INFO(module_name, "等待事件队列大小: {}", pending_events_.size());
    LOG_INFO(module_name, "已完成买单对应成交编号集合大小:{}", buy_order_done_ids_.size());
    LOG_INFO(module_name, "已完成卖单对应成交编号集合大小:{}", sell_order_done_ids_.size());
    LOG_INFO(module_name, "订单索引大小: {}", order_index_.size());
    LOG_INFO(module_name, "买盘档位数量: {}", bids_.size());
    LOG_INFO(module_name, "卖盘档位数量: {}", asks_.size());
    LOG_INFO(module_name, "历史涨停封单比例数量: {}", limit_up_fengdan_ratios_.size());
    LOG_INFO(module_name, "=========================================");
}

// 循环打印
void OrderBook::printloop(int level_num) {
    while (running_) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            printOrderBook(level_num);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 每3秒打印一次
    }
}

// 检查涨停撤单情况
void OrderBook::checkLimitUpWithdrawal(int timestamp) {
    if (is_send_){
        return; // 已经发送过警告, 不再重复发送
    }

    // 如果没有买盘则直接返回
    if (bid_volume_at_price_.empty()) {
        return;
    }

    int fake_limit_up_price = 0;
    int best_bid_price = 0;
    int best_ask_price = 0;
    int best_bid_volume = 0;
    int best_ask_volume = 0;

    auto best_bid_it = bid_volume_at_price_.rbegin();
    auto best_ask_it = ask_volume_at_price_.rbegin();

    if (best_bid_it != bid_volume_at_price_.rend()){
        best_bid_price = best_bid_it->first;
        best_bid_volume = best_bid_it->second;
    }

    if (best_ask_it != ask_volume_at_price_.rend()){
        best_ask_price = best_ask_it->first;
        best_ask_volume = best_ask_it->second;
    }

    // 涨停价假设
    fake_limit_up_price = std::max(best_bid_price, best_ask_price); 

    // 买一价不是涨停价, 重置最高买一量
    if (best_bid_price < fake_limit_up_price) {
        max_bid_volume_ = 0; 
        return;
    }

    // 买一金额 < 2000万, 不考虑涨停撤单判断
    const long long THRESHOLD = 200000000000LL; // 2000万 * 10000
    if (static_cast<long long>(best_bid_volume) * best_bid_price < THRESHOLD) {
        return; 
    }

    // 计算买一价位及以下的总卖单量
    int total_ask_volume_at_or_below_best_bid = 0;
    for (auto it = ask_volume_at_price_.begin(); it != ask_volume_at_price_.end(); ++it) {
        if (it->first <= best_bid_price) {
            total_ask_volume_at_or_below_best_bid += it->second;
        } else {
            break;
        }
    }

    // 计算封单量
    int fengdan_volume = best_bid_volume - total_ask_volume_at_or_below_best_bid;

    // 更新最大封单量
    if (fengdan_volume > max_bid_volume_){
        max_bid_volume_ = fengdan_volume;
        // LOG_INFO(module_name, "[{}] 创历史最高买一量: {}, 价格: {}", symbol_, max_bid_volume_, current_price / 10000.0);
        
        return; // 若发生更新, 肯定是买盘增加了, 直接返回
    }

    // 更新封单比例的时间窗口
    int old_event_timestamp = last_event_timestamp_ - 5000;
    for (auto it = limit_up_fengdan_ratios_.begin(); it != limit_up_fengdan_ratios_.end(); ++it) {
        if (it->first > old_event_timestamp) {
            break;
        }

        limit_up_fengdan_ratios_.erase(it);
    }

    // 计算当前封单比例
    double current_ratio = (max_bid_volume_ > 0) ? static_cast<double>(fengdan_volume) / max_bid_volume_ : 0.0;
    
    // 计算时间窗口内封单比例变化
    double ratio_change = 0.0;
    double max_ratio_in_window = 0.0;
    if (!limit_up_fengdan_ratios_.empty()) {
        for (const auto& [timestamp, ratio] : limit_up_fengdan_ratios_) {
            if (ratio > max_ratio_in_window) {
                max_ratio_in_window = ratio;
            }
        }
        ratio_change = max_ratio_in_window - current_ratio;
    }

    // 记录当前封单比例
    limit_up_fengdan_ratios_[timestamp] = current_ratio;

    // 撤单策略1
    auto s1 = [&](){
        if (max_bid_volume_ > 0 && current_ratio < (2.0 / 3.0) && ratio_change > 0.2) {

            // 放发生交易请求的部分
            if (send_server_) {
                send_server_->send(formatStockAccount(
                    symbol_, 
                    stock_with_accounts_
                ));
                is_send_ = true;
            }

            LOG_WARN(module_name, "[{}] 涨停撤单警告: 封单比例 {} --> 当前封单量 {} 低于历史最高封单量 {} 的 2/3 且 5s 内封单比例下{}, 5s最大封单比例:{}, 当前时间: {}", 
                symbol_, current_ratio, fengdan_volume, max_bid_volume_, ratio_change, max_ratio_in_window, last_event_timestamp_);
        }
    };

    // 9:19:50 - 9:20:00 执行策略
    // if (last_event_timestamp_ >= 33590000 && last_event_timestamp_ <= 33600000){
    //     s1();
    // }
    s1();
}

