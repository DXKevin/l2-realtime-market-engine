#include "L2HttpDownloader.h"
#include "L2Parser.h"
#include "nlohmann/json.hpp"
#include "Logger.h"
#include "FileOperator.h"
#include "Base64Decoder.h"





L2HttpDownloader::L2HttpDownloader(
    const std::string& base_url,
    const std::string& username,
    const std::string& password,
    std::unordered_map<std::string, std::unique_ptr<OrderBook>>& orderBooks_ref
) : base_url_(base_url),
    username_(username),
    password_(password),
    orderBooks_ref_(orderBooks_ref) 
{}

L2HttpDownloader::~L2HttpDownloader() {
    waitAll();
}

void L2HttpDownloader::run() {
    loginThread_ = std::thread(&L2HttpDownloader::loginLoop, this);
}

void L2HttpDownloader::login() {
    nlohmann::json login_data = {
        {"userName", username_},
        {"userPwd", password_}
    };

    httplib::Client cli(base_url_);
    auto res = cli.Post("/Login", login_data.dump(), "application/json");

    if (!res || res->status != 200) {
        LOG_ERROR("L2HttpDownloader", "登录失败，无法连接到服务器或响应错误");
        return;
    }

    nlohmann::json res_json = nlohmann::json::parse(res->body);
    if (res_json["Code"] == 200 && res_json["Data"]["code"] == 0) {
        // 提取 Cookie
        std::string cookie_header = res->get_header_value("Set-Cookie");
        if (!cookie_header.empty()) {
            size_t start = cookie_header.find("__beetlex_token=");
            if (start != std::string::npos) {
                start += 16;
                size_t end = cookie_header.find(";", start);
                if (end == std::string::npos) end = cookie_header.length();
                std::string token = cookie_header.substr(start, end - start);
                
                cookie_ = "__beetlex_token=" + token;

                is_logined_.store(true);
                LOG_INFO("L2HttpDownloader", "登录HTTP服务器成功");
            } else {
                LOG_ERROR("L2HttpDownloader", "登录成功，但响应头中没有 '__beetlex_token'");
                return;
            }
        } else {
            LOG_ERROR("L2HttpDownloader", "登录成功，但响应头中没有 'Set-Cookie'");
            return;
        }
 
    } else {
        int error_code = res_json["Data"]["code"];
        LOG_ERROR("L2HttpDownloader", "登录失败，服务器返回错误代码: {}", error_code);
        return;
    }
}

void L2HttpDownloader::loginLoop() {
    while (true) {
        if (!is_logined_.load()) {
            login();
        }   
        
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

void L2HttpDownloader::start_download_async(const std::string& symbol, const std::string& type) {
    auto task = std::async(std::launch::async, [this, symbol, type]() {
        download_and_parse(symbol, type);
    });
    
    std::lock_guard<std::mutex> lock(mtx_);
    pending_tasks_.emplace_back(std::move(task));
}

void L2HttpDownloader::download_and_parse(const std::string& symbol, const std::string& type) {
    std::string result = download(symbol, type);

    // std::string result = "";
    // if (type == "Order") { 
    //     result = readCsvFile("data/20260109_Order_600895.SH.csv");
    // } else if (type == "Tran") {
    //     result = readCsvFile("data/20260109_Tran_600895.SH.csv");
    // }

    parse_data(symbol, type, result);
}

std::string L2HttpDownloader::download(const std::string& symbol, const std::string& type) {
    httplib::Client cli(base_url_);
    httplib::Headers headers = {
        {"Cookie", cookie_}
    };

    nlohmann::json req_data = {
        {"stock", symbol},
        {"type", type}
    };

    auto res = cli.Post("/GetData", headers, req_data.dump(), "application/json");

    if (!res || res->status != 200) {
        LOG_ERROR("L2HttpDownloader", "下载数据失败，无法连接到服务器或响应错误");
        return "";
    }

    nlohmann::json res_json = nlohmann::json::parse(res->body);
    if (res_json["Code"] != 200) {
        
        int error_code = res_json["Code"];
        LOG_ERROR("L2HttpDownloader", "下载数据失败，服务器返回错误代码: {}", error_code);
        return "";
    }

    if (res_json["Data"]["code"] == -1) {
        std::string msg = res_json["Data"]["msg"];
        LOG_ERROR("L2HttpDownloader", "下载数据失败，服务器返回错误消息: {}", msg);
        return "";
    }

    // 提取并解压gzip数据
    std::string data = res_json["Data"]["data"];

    auto decoded = base64_decode(data);
    auto decompressed = gzip_decompress(decoded);

    std::string result(decompressed.begin(), decompressed.end());

    if (type == "Order") {
        std::string filename = "data/" + symbol + "_order_http.txt";
        writeTxtFile(filename, result);
    } else if (type == "Tran") {
        std::string filename = "data/" + symbol + "_tran_http.txt";
        writeTxtFile(filename, result);
    }
    
    return result;
}

void L2HttpDownloader::parse_data(const std::string& symbol, const std::string& type, const std::string_view result_view) {
    // 解析数据
    size_t pos = 0;
    constexpr size_t ORDER_FIELDS = 14;
    constexpr size_t TRADE_FIELDS = 15;

    auto it = orderBooks_ref_.find(symbol);
    if (it == orderBooks_ref_.end()) {
        LOG_WARN("L2HttpDownloader", "未找到对应的 OrderBook 处理数据，合约代码: {}", symbol);
        return;
    }

    LOG_INFO("L2HttpDownloader", "开始解析HTTP数据, 合约代码: {}, 类型: {}, 数据大小: {} 字节", symbol, type, result_view.size());
    while (pos < result_view.size()) {
        size_t next = result_view.find('\n', pos);

        if (pos == 0) {
            // 跳过首行表头
            pos = next + 1;
            continue;
        }

        std::string_view line;
        if (next == std::string_view::npos) {
            line = result_view.substr(pos);
        } else {
            line = result_view.substr(pos, next - pos);
        }

        auto fields = splitByComma(line);

        if (type == "Order") {
            if  (fields.size() == ORDER_FIELDS) {
                std::vector<std::string_view> relevant_fields = {
                    fields[0], fields[1], fields[2], fields[3], fields[4], fields[5],
                    fields[6], fields[7], fields[8], fields[9], fields[10], fields[11], fields[12]
                };
                
                MarketEvent event = MarketEvent(L2Order(relevant_fields));

                const auto& order = std::get<L2Order>(event.data);
                it->second->pushHistoryEvent(event);

                // if (order.timestamp < 42000000) {
                //     it->second->pushHistoryEvent(event);
                // }
            } else {
                LOG_WARN("L2HttpDownloader", "order字段数不匹配, data:{} --> size:{}", line, fields.size());
            }
        } else if (type == "Tran") {
            if (fields.size() == TRADE_FIELDS) {
                std::vector<std::string_view> relevant_fields = {
                    fields[0], fields[1], fields[2], fields[3], fields[4], fields[5],
                    fields[6], fields[7], fields[8], fields[9], fields[10], 
                    fields[11], fields[12], fields[13]
                };

                MarketEvent event = MarketEvent(L2Trade(relevant_fields));

                const auto& trade = std::get<L2Trade>(event.data);
                it->second->pushHistoryEvent(event);

                // if (trade.timestamp < 42000000) {
                //     it->second->pushHistoryEvent(event);
                // }
            } else {
                LOG_WARN("L2HttpDownloader", "trade字段数不匹配, data:{} --> size:{}", line, fields.size());
            }
        }
        pos = next + 1;
    }

    // 标记历史数据下载处理完成
    if (type == "Order") {
        it->second->is_history_order_done_.store(true);
    } else if (type == "Tran") {
        it->second->is_history_trade_done_.store(true);
    }
}

void L2HttpDownloader::waitAll() {
    std::vector<std::future<void>> tasks;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        tasks = std::move(pending_tasks_);
    }
    for (auto& task : tasks) {
        try {
            task.get(); // 阻塞并获取结果（可捕获异常）
        } catch (const std::exception& e) {
            LOG_ERROR("L2HttpDownloader", "下载任务异常: {}", e.what());
        }
    }

    if  (loginThread_.joinable()) {
        loginThread_.join();
    }
}