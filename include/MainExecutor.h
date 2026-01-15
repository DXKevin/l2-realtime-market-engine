#include "concurrentqueue/blockingconcurrentqueue.h"
#include "L2TcpSubscriber.h"
#include "SendServer.h"
#include "L2HttpDownloader.h"

#include <thread>

class MainExecutor {
public:
    MainExecutor(
         moodycamel::BlockingConcurrentQueue<std::string>& monitorEventQueue,
         std::unordered_map<std::string, std::unique_ptr<OrderBook>>& orderBooks,
         std::unordered_map<std::string, std::vector<std::string>>& stockWithAccounts,
         SendServer& sendServer,
         L2TcpSubscriber& orderSubscriber,
         L2TcpSubscriber& tradeSubscriber,
         L2HttpDownloader& downloader
    );
    ~MainExecutor();
    
    void run();
private:
    bool isLogined();
    void handleMonitorEvent(const std::string& event);
    
    moodycamel::BlockingConcurrentQueue<std::string>& monitorEventQueue_;
    std::unordered_map<std::string, std::unique_ptr<OrderBook>>& orderBooks_;
    std::unordered_map<std::string, std::vector<std::string>>& stockWithAccounts_;
    SendServer& sendServer_;
    L2TcpSubscriber& orderSubscriber_;
    L2TcpSubscriber& tradeSubscriber_;
    L2HttpDownloader& downloader_;

    std::thread monitorEventThread_;
};