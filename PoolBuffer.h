#pragma once

#include <array>
#include <mutex>
#include <condition_variable>
#include <cstddef>
#include <stdexcept>
#include <cassert>
#include <functional>

/**
 * @brief 固定大小的缓冲池（对象池语义）
 *
 * @tparam T  元素类型
 * @tparam N  池中槽位总数（编译期常量，固定不变）
 *
 * 设计语义
 * --------
 *  - 池中有 N 个固定槽位，每个槽位有两种状态：空闲 / 已借出。
 *  - acquire()  从池中借出一个空闲槽位，返回其元素指针，并将其标记为"已借出"。
 *               若池中无空闲槽位，可选择阻塞等待或立即返回 nullptr。
 *  - release(p) 归还一个已借出的槽位，将其重新标记为"空闲"，
 *               并唤醒正在 acquire() 中等待的线程。
 *  - 调用者通过返回的指针直接读写槽位内容，池本身不关心数据语义。
 *
 * 线程安全
 * --------
 *  - acquire() / release() 内部已加锁，天然线程安全。
 *  - 持有指针期间直接操作数据不需要额外加锁（各线程持有不同槽位指针）。
 *  - 若多个线程共享同一个槽位指针，调用者自行负责同步。
 *  - 额外提供 lock() / unlock() / try_lock() 供需要原子性批量操作时使用。
 *
 * 典型用法
 * --------
 * @code
 *   PoolBuffer<MyData, 8> pool;
 *
 *   // 借出
 *   MyData* p = pool.acquire();       // 阻塞直到有空闲槽位
 *   if (p) {
 *       p->field = value;             // 直接操作
 *       send(p);                      // 传递给其他模块使用
 *   }
 *
 *   // 使用完毕后归还
 *   pool.release(p);                  // 槽位重新变为空闲
 * @endcode
 */
template <typename T, std::size_t N>
class PoolBuffer {
    static_assert(N > 0, "PoolBuffer: capacity N must be > 0");

public:
    using value_type    = T;
    using size_type     = std::size_t;
    using pointer       = T*;
    using const_pointer = const T*;

    // ---------------------------------------------------------------- 构造
    PoolBuffer() : free_count_(N) {
        // 初始化：所有槽位均为空闲
        for (size_type i = 0; i < N; ++i) {
            used_[i] = false;
        }
    }

    // mutex / condvar 不可复制或移动
    PoolBuffer(const PoolBuffer&)            = delete;
    PoolBuffer& operator=(const PoolBuffer&) = delete;
    PoolBuffer(PoolBuffer&&)                 = delete;
    PoolBuffer& operator=(PoolBuffer&&)      = delete;

    /**
     * @brief 使用 lambda 对池中所有元素进行初始化（推荐方式）
     *
     * @param init_func 接收 (T& element, size_t index) 的初始化函数
     *                  在构造后、任何 acquire() 之前调用最合适
     *
     * 示例：
     *   PoolBuffer<FrameData, 12> pool;
     *   pool.initialize([](FrameData& frame, size_t idx) {
     *       frame.index = idx;
     *       frame.width = 1920;
     *       frame.height = 1080;
     *       frame.data.resize(1920*1080*2);  // NV12 等
     *   });
     */
    void initialize(std::function<void(T&, size_type)> init_func) {
        std::lock_guard<std::mutex> lk(mutex_);   // 保证线程安全
        for (size_type i = 0; i < N; ++i) {
            init_func(buffer_[i], i);
        }
    }

    /**
     * @brief 简单遍历版本（只传引用，不传索引）
     */
    void initialize(std::function<void(T&)> init_func) {
        std::lock_guard<std::mutex> lk(mutex_);
        for (size_type i = 0; i < N; ++i) {
            init_func(buffer_[i]);
        }
    }

    // ---------------------------------------------------------------- 借出

    /**
     * @brief 借出一个空闲槽位（阻塞版本）
     *
     * 若当前无空闲槽位，调用线程将阻塞，直到其他线程归还某个槽位为止。
     *
     * @return 指向该槽位元素的指针，保证非 nullptr
     */
    pointer acquire() {
        std::unique_lock<std::mutex> lk(mutex_);
        // 等待直到有空闲槽位
        cv_.wait(lk, [this]{ return free_count_ > 0; });
        return acquire_impl();
    }

    /**
     * @brief 尝试借出一个空闲槽位（非阻塞版本）
     *
     * 若当前无空闲槽位，立即返回 nullptr，不阻塞。
     *
     * @return 指向空闲槽位的指针；若无空闲槽位则返回 nullptr
     */
    pointer try_acquire() {
        std::lock_guard<std::mutex> lk(mutex_);
        if (free_count_ == 0) return nullptr;
        return acquire_impl();
    }

    /**
     * @brief 带超时的借出（超时返回 nullptr）
     *
     * @param timeout_ms 最长等待毫秒数
     * @return 指向空闲槽位的指针；超时则返回 nullptr
     */
    pointer acquire_for(unsigned int timeout_ms) {
        std::unique_lock<std::mutex> lk(mutex_);
        bool ok = cv_.wait_for(
            lk,
            std::chrono::milliseconds(timeout_ms),
            [this]{ return free_count_ > 0; }
        );
        if (!ok) return nullptr;
        return acquire_impl();
    }

    // ---------------------------------------------------------------- 归还

    /**
     * @brief 归还一个已借出的槽位
     *
     * @param p 由 acquire() / try_acquire() 返回的指针
     * @throws std::invalid_argument 若 p 不属于本池，或该槽位并未处于借出状态
     */
    void release(pointer p) {
        if (!p) return;

        // 计算物理槽位下标
        std::ptrdiff_t diff = p - buffer_.data();
        if (diff < 0 || static_cast<size_type>(diff) >= N) {
            throw std::invalid_argument(
                "PoolBuffer::release: pointer does not belong to this pool");
        }
        size_type idx = static_cast<size_type>(diff);

        {
            std::lock_guard<std::mutex> lk(mutex_);
            if (!used_[idx]) {
                throw std::invalid_argument(
                    "PoolBuffer::release: slot is not currently acquired");
            }
            used_[idx] = false;
            ++free_count_;
        }
        // 通知一个正在等待的 acquire()
        cv_.notify_one();
    }

    // ---------------------------------------------------------------- 状态查询

    /** 当前空闲槽位数量（线程安全） */
    size_type free_count() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return free_count_;
    }

    /** 当前已借出槽位数量（线程安全） */
    size_type used_count() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return N - free_count_;
    }

    /** 池是否已全部借出（线程安全） */
    bool exhausted() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return free_count_ == 0;
    }

    /** 池容量（编译期常量） */
    constexpr size_type capacity() const noexcept { return N; }

    // ---------------------------------------------------------------- 外部锁接口（批量操作用）

    /**
     * @brief 加锁（阻塞等待）
     *
     * 当需要原子性地执行"查询状态 + 借出/归还"组合操作时使用。
     * 注意：持锁期间不能再调用内部已加锁的 acquire()/release()，
     *       应改为调用无锁的 acquire_unlocked()/release_unlocked()。
     */
    void lock()         const { mutex_.lock();   }
    void unlock()       const { mutex_.unlock(); }
    bool try_lock()     const { return mutex_.try_lock(); }

    // ---------------------------------------------------------------- 无锁版本（在外部已持锁时使用）

    /**
     * @brief acquire 的无锁版本，须在外部 lock() 后调用
     *
     * @return 空闲槽位指针；若无空闲槽位则返回 nullptr
     */
    pointer acquire_unlocked() {
        if (free_count_ == 0) return nullptr;
        return acquire_impl();
    }

    /**
     * @brief release 的无锁版本，须在外部 lock() 后调用
     *
     * @param p 由 acquire 系列函数返回的指针
     */
    void release_unlocked(pointer p) {
        if (!p) return;
        std::ptrdiff_t diff = p - buffer_.data();
        assert(diff >= 0 && static_cast<size_type>(diff) < N);
        size_type idx = static_cast<size_type>(diff);
        assert(used_[idx]);
        used_[idx] = false;
        ++free_count_;
        // 注意：外部持锁时不能 notify（死锁风险），
        // 应在 unlock() 后手动调用 notify_waiters()
    }

    /**
     * @brief 在外部解锁后调用，唤醒等待中的 acquire() 线程
     */
    void notify_waiters() {
        cv_.notify_one();
    }

private:
    // 内部无锁借出实现（调用方须已持锁且 free_count_ > 0）
    pointer acquire_impl() {
        for (size_type i = 0; i < N; ++i) {
            if (!used_[i]) {
                used_[i] = true;
                --free_count_;
                return &buffer_[i];
            }
        }
        // 不应到达此处（调用前已确认 free_count_ > 0）
        assert(false);
        return nullptr;
    }

    std::array<T, N>        buffer_;      ///< 固定槽位存储
    std::array<bool, N>     used_;        ///< 各槽位借出标记
    size_type               free_count_;  ///< 当前空闲槽位数量
    mutable std::mutex      mutex_;       ///< 互斥锁
    std::condition_variable cv_;          ///< 用于阻塞等待空闲槽位
};
