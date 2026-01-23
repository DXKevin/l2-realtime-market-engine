#include "Executor.h"
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

class ExecutorManager {
public:
    void run() {
        while (running_) {
            executor_instance_ = std::make_unique<Executor>();
            
            // 启动 Executor
            executor_instance_->start();

            // 循环检查 Executor 是否需要重启
            while (running_ && !executor_instance_->needsReset()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 避免忙等待
            }

            if (running_ && executor_instance_->needsReset()) {
                LOG_INFO("ExecutorManager", "Executor is stopping...");
                executor_instance_->stop(); // 停止当前实例
                LOG_INFO("ExecutorManager", "Executor stopped.");
                executor_instance_.reset(); // 销毁当前实例
                LOG_INFO("ExecutorManager", "Executor reset.");
                
                // 添加延迟，避免频繁重启
                std::this_thread::sleep_for(std::chrono::seconds(2));
                
                // 继续循环，创建新实例
                continue;
            }

            // 如果 running_ 变为 false，则正常退出
            break;
        }
        
        // 确保在 Manager 结束前停止并销毁 Executor
        if (executor_instance_) {
             executor_instance_->stop();
             executor_instance_.reset();
        }
        LOG_INFO("ExecutorManager", "Manager stopped.");
    }

    void stop() {
        running_.store(false);
    }

private:
    std::unique_ptr<Executor> executor_instance_;
    std::atomic<bool> running_{true};
};