#pragma once

#include <httplib/httplib.h>
#include <future>
#include <mutex>

#include "OrderBook.h"

class L2HttpDownloader { 
public:
    L2HttpDownloader(
        const std::string& base_url,
        const std::string& username,
        const std::string& password,
        std::shared_ptr<std::unordered_map<std::string, std::unique_ptr<OrderBook>>> orderbooks_ptr
    );

    ~L2HttpDownloader();

    void login();
    
    void start_download_async(const std::string& symbol, const std::string& type);
    void download(const std::string& symbol, const std::string& type);
    void waitAll();

private:
    std::string base_url_;
    std::string username_;
    std::string password_;
    std::string cookie_;

    std::shared_ptr<std::unordered_map<std::string, std::unique_ptr<OrderBook>>> orderbooks_ptr_;

    mutable std::mutex mtx_;
    std::vector<std::future<void>> pending_tasks_;


};