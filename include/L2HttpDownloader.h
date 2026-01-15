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
        std::unordered_map<std::string, std::unique_ptr<OrderBook>>& orderBooks_ref
    );

    ~L2HttpDownloader();

    void run();

    void login();
    void loginLoop();
    
    void start_download_async(const std::string& symbol, const std::string& type);
    void download_and_parse(const std::string& symbol, const std::string& type);
    std::string download(const std::string& symbol, const std::string& type);
    void parse_data(const std::string& symbol, const std::string& type, const std::string_view result_view);
    void waitAll();
    

    std::atomic<bool> is_logined_{false};
private:
    std::string base_url_;
    std::string username_;
    std::string password_;
    std::string cookie_;

    std::unordered_map<std::string, std::unique_ptr<OrderBook>>& orderBooks_ref_;

    mutable std::mutex mtx_;
    std::vector<std::future<void>> pending_tasks_;

    std::thread loginThread_;

    
};