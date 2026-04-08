#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <sys/time.h>
#include <cstdint>
#include <atomic>

template<typename T>
class ThreadSafeBoundedQueue {
public:
    enum class FullPolicy {
        Drop,       // 队列满时直接丢弃新数据（不阻塞生产者）
        Overwrite   // 队列满时丢弃最旧的数据，放入最新数据
    };

    explicit ThreadSafeBoundedQueue(size_t max_size, FullPolicy policy = FullPolicy::Overwrite)
        : max_size_(max_size), policy_(policy) {
        if (max_size_ == 0)
            max_size_ = 1;  // 至少能存一个
    }
    virtual ~ThreadSafeBoundedQueue() = default;

    // 禁止拷贝，允许移动
    ThreadSafeBoundedQueue(const ThreadSafeBoundedQueue&) = delete;
    ThreadSafeBoundedQueue& operator=(const ThreadSafeBoundedQueue&) = delete;
    ThreadSafeBoundedQueue(ThreadSafeBoundedQueue&&) = default;
    ThreadSafeBoundedQueue& operator=(ThreadSafeBoundedQueue&&) = default;

    // ==================== 生产者接口 ====================

    // 普通 push（根据策略决定是否真的放入）
    bool push(const T& value) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (closed_) return false;

        if (queue_.size() >= max_size_) {
            if (policy_ == FullPolicy::Drop) {
                dropped_++;                 // 可统计丢弃数量
                return false;
            } else { // Overwrite
                queue_.pop();                   // 丢弃最旧
                dropped_++;                     // 也可以单独统计 overwrite 次数
            }
        }
        queue_.push(value);
        cond_.notify_one();
        return true;
    }

    bool push(T&& value) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (closed_) return false;

        if (queue_.size() >= max_size_) {
            if (policy_ == FullPolicy::Drop) {
                dropped_++;
                return false;
            } else {
                queue_.pop();
                dropped_++;
            }
        }
        queue_.push(std::move(value));
        cond_.notify_one();
        return true;
    }

    // ==================== 消费者接口 ====================

    // 阻塞 pop，直到有数据或队列关闭
    bool pop(T& value) {
        std::unique_lock<std::mutex> lk(mtx_);
        cond_.wait(lk, [this] { return !queue_.empty() || closed_; });

        if (queue_.empty()) return false;  // closed && empty

        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // 非阻塞尝试获取
    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (queue_.empty()) return false;
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // 超时 pop（毫秒）
    bool timed_pop(T& value, uint64_t timeout_ms) {
        std::unique_lock<std::mutex> lk(mtx_);

        // 使用 wait_for 代替 wait_until，直接传入毫秒数
        // std::chrono::milliseconds 会自动处理时间转换
        bool ok = cond_.wait_for(lk, std::chrono::milliseconds(timeout_ms), [this] {
            return !queue_.empty() || closed_;
        });

        // 如果超时或队列在关闭时仍为空，返回 false
        if (!ok || queue_.empty()) {
            return false;
        }

        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // ==================== 控制接口 ====================

    void close() {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            closed_ = true;
        }
        cond_.notify_all();
    }

    bool is_closed() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return closed_;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return queue_.empty();
    }

    size_t capacity() const { return max_size_; }

    // 获取被丢弃的数据数量（Drop 或 Overwrite 都会计数）
    uint64_t dropped_count() const { return dropped_.load(); }
    void reset_dropped() { dropped_ = 0; }

protected:
    std::queue<T>           queue_;
    mutable std::mutex      mtx_;
    std::condition_variable cond_;

    size_t            max_size_;
    FullPolicy        policy_;

    bool                    closed_ {false};
    std::atomic<uint64_t>   dropped_ {0};   // 被丢弃的数量
};
