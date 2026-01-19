#pragma once

#include <atomic>
#include <fstream>
#include <objidl.h>
#include <string>
#include <thread>
#include <iostream>
#include <utility>

#include "Logger.h"
#include "concurrentqueue/blockingconcurrentqueue.h"

class AsyncFileWriter {
private:
    struct WriteTask {
        std::string filename;
        std::string content;
    };

    moodycamel::BlockingConcurrentQueue<WriteTask> task_queue; // 使用阻塞队列
    std::thread writer_thread;
    std::atomic_bool running{true};

    void perform_write(const std::string& filename, const std::string& content) {
        std::ofstream file(filename, std::ios::app);
        if (file.is_open()) {
            file << content;
            file.close();
        } else {
            LOG_ERROR("AsyncFileWriter", "无法打开文件 {} 进行写入", filename);
        }
    }

    void writer_loop() {
        while (running) {
            WriteTask task;
            task_queue.wait_dequeue(task);

            // 检查是否是哨兵任务
            if (task.filename.empty() && task.content.empty()) {
                break; 
            }
            perform_write(task.filename, task.content);
        }
    }

public:
    AsyncFileWriter() {
        writer_thread = std::thread(&AsyncFileWriter::writer_loop, this);
    }

    ~AsyncFileWriter() {
        stop();
    }

    void write_async(std::string filename, std::string content) {
        task_queue.enqueue({std::move(filename), std::move(content)});
    }

    void stop() {
        running.store(false);
        task_queue.enqueue({});
        if (writer_thread.joinable()) {
            writer_thread.join();
        }
    }
};