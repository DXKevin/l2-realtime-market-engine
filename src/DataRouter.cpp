#include "DataRouter.h"
#include "L2Parser.h"
#include "Logger.h"



DataRouter::DataRouter(
    std::unordered_map<std::string, std::unique_ptr<OrderBook>>& orderBooks_ref,
    AsyncFileWriter& asyncFileWriter_ref
): 
    orderBooks_ref_(orderBooks_ref),
    asyncFileWriter_ref_(asyncFileWriter_ref)
{
    running_ = true;
    worker_thread_ = std::thread(&DataRouter::worker, this);
}

DataRouter::~DataRouter() {
    running_ = false;
    worker_thread_.join();
}

void DataRouter::pushData(const DataMessage& data_message) {
    eventQueue_.enqueue(data_message);
}

void DataRouter::worker() {
    while (running_) {
        DataMessage data_message;
        eventQueue_.wait_dequeue(data_message);

        auto events = parseL2Data(data_message.data_, data_message.type_, buffer_, asyncFileWriter_ref_);

        for (const auto &event : events) {
            const std::string &symbol = getSymbol(event);
            auto it = orderBooks_ref_.find(symbol);
            if (it != orderBooks_ref_.end()) {
                it->second->pushEvent(event);
            } else {
                LOG_WARN("DataRouter", "未找到对应的 OrderBook 处理数据，合约代码: {}", symbol);
            }
        }
    }
}

void DataRouter::stop() {
    running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}