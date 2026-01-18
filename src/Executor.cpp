#include "Executor.h"
#include "Logger.h"
#include "ConfigReader.h"
#include "OrderBook.h"
#include "SendServer.h"
#include "L2TcpSubscriber.h"
#include "L2HttpDownloader.h"
#include "ReceiveServer.h"
#include "L2Parser.h"

Executor::Executor() {
    
}
Executor::~Executor() {
    stop();
}

void Executor::start() {
    init();

    running_.store(true);
    monitorEventThread_ = std::thread(&Executor::monitorEventLoop, this);
}

void Executor::stop() {
    running_.store(false);
    if (monitorEventThread_.joinable()) {
        monitorEventThread_.join();
    }
}

void Executor::init() {
    module_name_ = "Executor";

    // 读取配置文件
    ConfigReader config("config.ini");
    http_url_ = config.get("server", "http_url");
    tcp_host_ = config.get("server", "tcp_host");
    order_port_ = config.getInt("server", "order_port");
    trade_port_ = config.getInt("server", "trade_port");
    username_ = config.get("auth", "username");
    password_ = config.get("auth", "password");
    
    orderBooks_ = std::make_unique<
        std::unordered_map<std::string, std::unique_ptr<OrderBook>>
    >();

    stockWithAccounts_ = std::make_unique<
        AutoSaveJsonMap<std::string, std::vector<int>>
    >("stocks_monitor.json");

    monitorEventQueue_ = std::make_unique<
        moodycamel::BlockingConcurrentQueue<std::string>
    >();

    // 初始化交易信号发送服务器
    sendServer_ = std::make_unique<SendServer>("to_python_pipe");

    // 初始化HTTP下载器
    downloader_ = std::make_unique<L2HttpDownloader>(
        http_url_,
        username_,
        password_,
        *orderBooks_
    );

    // 初始化行情服务器连接
    orderSubscriber_ = std::make_unique<L2TcpSubscriber>(
        tcp_host_, 
        order_port_, 
        username_, 
        password_, 
        "order", 
        *orderBooks_
    );
    tradeSubscriber_ = std::make_unique<L2TcpSubscriber>(
        tcp_host_, 
        trade_port_, 
        username_, 
        password_, 
        "trade", 
        *orderBooks_
    );

    // 初始化接收前端消息服务器
    recvServer_ = std::make_unique<ReceiveServer>(
        "from_nodejs_pipe", 
        *monitorEventQueue_
    );

    orderSubscriber_->run();
    tradeSubscriber_->run();

    LOG_INFO(module_name_, "程序初始化完成");
}

void Executor::monitorEventLoop() {
    while (running_.load()) {
        // 检测是否登录服务器
        if (!isLogined()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        // 获取所有监控的股票代码
        std::vector<std::string> symbols;
        stockWithAccounts_->forEach([&symbols](const std::string& symbol, const auto&) {
            symbols.push_back(symbol);  // forEach 遍历时不能修改 map，否则会导致死锁, 所以先复制 key
        });

        // 初始化本地历史监控任务
        for (const std::string& symbol : symbols) {
            handleMonitorEvent(symbol);
        }

        // 等待前端监控消息 
        while (isLogined()) { 
            LOG_INFO(module_name_, "等待前端监控消息...");
            std::string event;
            monitorEventQueue_->wait_dequeue(event);
            std::string symbol = parseAndStoreStockAccount(event, *stockWithAccounts_);
            if (symbol.empty()) {
                LOG_WARN(module_name_, "从前端消息解析出股票代码为空: {}", event);
                continue;
            }

            // 处理监控事件
            handleMonitorEvent(symbol);
        }

        // 服务器登录断开，请求重启
        requestReset();
    }
}

bool Executor::isLogined() {
    return orderSubscriber_->is_logined_ && 
            tradeSubscriber_->is_logined_;
}

void Executor::handleMonitorEvent(const std::string& symbol) {
    


    if (!isLogined()) {
        LOG_WARN(module_name_, "未登录行情服务器，跳过处理股票代码 {} 的监控事件", symbol);
        return;
    }

    if (orderBooks_->find(symbol) != orderBooks_->end()) {
        LOG_INFO(module_name_, "股票代码 {} 的 OrderBook 已存在，跳过创建新实例", symbol);
        return;
    }

    orderBooks_->emplace(
        symbol, 
        std::make_unique<OrderBook>(
            symbol, 
            *sendServer_,
            *stockWithAccounts_
        )
    );

    // 订阅逐笔委托和逐笔成交
    orderSubscriber_->subscribe(symbol); 
    tradeSubscriber_->subscribe(symbol); 

    // 等待10秒
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // 下载历史数据
    downloader_->start_download_async(symbol, "Order"); 
    downloader_->start_download_async(symbol, "Tran");
}
