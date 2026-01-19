#pragma once
#include "concurrentqueue/blockingconcurrentqueue.h"
#include "OrderBook.h"
#include "AsyncFileWriter.h"
#include "DataStruct.h"

class DataRouter {
public: 
    DataRouter(
        std::unordered_map<std::string, std::unique_ptr<OrderBook>>& orderBooks_ref,
        AsyncFileWriter& asyncFileWriter_ref
    );
    ~DataRouter();

    void stop();
    void pushData(const DataMessage& data_message);

private:
    void worker();

    std::string buffer_; // 数据缓冲区, 用于存储拆包非完整数据
    std::atomic<bool> running_;
    std::thread worker_thread_;
    moodycamel::BlockingConcurrentQueue<DataMessage> eventQueue_;

    // 外部传入对象
    AsyncFileWriter& asyncFileWriter_ref_;
    std::unordered_map<std::string, std::unique_ptr<OrderBook>>& orderBooks_ref_;

    


};