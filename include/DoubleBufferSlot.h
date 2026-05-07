#pragma once
#include <atomic>
#include <type_traits>

template<typename T>
class DoubleBufferSlot {
public:
    // 构造函数：允许用初始值初始化两个缓冲区（可选）
    explicit DoubleBufferSlot(const T& initial_value = T{}) {
        buffer_[0] = initial_value;
        buffer_[1] = initial_value;
    }

    // 更新数据（写线程调用）
    void update(const T& new_data) {
        int inactive = 1 - active_.load(std::memory_order_relaxed);
        buffer_[inactive] = new_data;                     // 写入非激活副本
        active_.store(inactive, std::memory_order_release); // 原子切换
    }

    // 读取当前有效数据（读线程调用）
    T read() const {
        int current = active_.load(std::memory_order_acquire);
        return buffer_[current];
    }

private:
    static_assert(std::is_copy_assignable_v<T>, "T must be copy-assignable");
    static_assert(std::is_default_constructible_v<T>, "T must be default-constructible (for default init)");

    T buffer_[2];                          // 两个泛型副本
    mutable std::atomic<int> active_{0};   // 当前激活索引
};