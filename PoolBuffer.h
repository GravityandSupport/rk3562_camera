#pragma once

#include <array>
#include <mutex>
#include <condition_variable>
#include <cstddef>
#include <stdexcept>
#include <cassert>
#include <atomic>
#include <functional>

/**
 * @brief 带引用计数的固定大小缓冲池
 *
 * @tparam T  元素类型
 * @tparam N  池中槽位总数（编译期常量）
 *
 * 设计语义
 * --------
 *  - 池中有 N 个固定 Slot，每个 Slot 内部包含：
 *      · data        —— 用户数据（类型 T）
 *      · ref_count   —— 引用计数（原子，初始为 0）
 *      · in_use      —— 借出标记
 *      · owner       —— 指回所属池，用于 ref_count 归零时自动归还
 *
 *  - acquire()  借出一个空闲 Slot，返回 Slot* 指针：
 *      · 将 in_use 置为 true，ref_count 置为 1
 *      · 调用者通过 slot->data 访问数据
 *
 *  - Slot::retain()   引用计数 +1（线程安全）
 *  - Slot::release()  引用计数 -1，若归零则自动将 Slot 归还池（线程安全）
 *
 *  - 池的 release(slot*)  强制归还（无视引用计数，用于异常清理等场景）
 *
 * 线程安全
 * --------
 *  - acquire / 池级 release 内部持 pool mutex。
 *  - ref_count 使用 std::atomic<int>，retain/release 无需额外加锁。
 *  - ref_count 归零时，release 内部会重新持 pool mutex 完成归还并 notify。
 */

struct ISlot {
public:
    virtual void retain() = 0;
    virtual void release() = 0;
    virtual void* getdata() = 0;
    virtual int ref_count() const = 0;

    void lock()          { mutex_.lock();        }
    void unlock()        { mutex_.unlock();      }
    bool try_lock()      { return mutex_.try_lock(); }

    virtual ~ISlot() {}

private:
    std::mutex      mutex_;
};

template <typename T, std::size_t N>
class PoolBuffer {
    static_assert(N > 0, "PoolBuffer: N must be > 0");

public:
    // 前向声明
    struct Slot;

    using value_type = T;
    using size_type  = std::size_t;

    // ================================================================ Slot
    /**
     * @brief 缓冲池的基本单元
     *
     * 持有用户数据、引用计数及归属池指针。
     * 调用者只持有 Slot 指针，通过 slot->data 操作数据，
     * 通过 slot->retain() / slot->release() 管理生命周期。
     */
    struct Slot  : public ISlot{
    public:
        T             data;       ///< 用户数据，直接读写
        PoolBuffer*   pool;       ///< 所属池（由池在 acquire 时注入）

        /**
         * @brief 引用计数 +1
         *
         * 每新增一个持有者时调用。线程安全。
         */
        void retain() override {
            ref_count_.fetch_add(1, std::memory_order_relaxed);
        }

        /**
         * @brief 引用计数 -1
         *
         * 当引用计数降为 0 时，自动将本 Slot 归还给所属池。
         * 线程安全。
         */
        void release() override {
            // fetch_sub 返回减之前的值
            int prev = ref_count_.fetch_sub(1, std::memory_order_acq_rel);
            assert(prev >= 1 && "release() called more times than retain()");
            if (prev == 1) {
                // 引用计数已归零，自动归还
                pool->return_slot(this);
            }
        }

        /**
         * @brief 当前引用计数快照（仅供调试/日志）
         */
        int ref_count() const override{
            return ref_count_.load(std::memory_order_relaxed);
        }

        void* getdata() override {
            return &data;
        }
    private:
        friend class PoolBuffer;   // 允许池直接操作私有成员

        std::atomic<int> ref_count_{0};   ///< 引用计数，acquire 时置 1
        bool             in_use_{false};  ///< 是否已借出（受 pool mutex 保护）

    public:
        Slot() : pool(nullptr) {}
    private:

        // 不允许外部拷贝/移动（含 atomic）
        Slot(const Slot&)            = delete;
        Slot& operator=(const Slot&) = delete;
    };

    // ================================================================ 构造
    PoolBuffer() : free_count_(N) {
        for (size_type i = 0; i < N; ++i) {
            slots_[i].pool    = this;
            slots_[i].in_use_ = false;
            slots_[i].ref_count_.store(0, std::memory_order_relaxed);
        }
    }

    PoolBuffer(const PoolBuffer&)            = delete;
    PoolBuffer& operator=(const PoolBuffer&) = delete;
    PoolBuffer(PoolBuffer&&)                 = delete;
    PoolBuffer& operator=(PoolBuffer&&)      = delete;

    void initialize(std::function<void(T&, size_type)> init_func){
        std::lock_guard<std::mutex> lk(mutex_);   // 保证线程安全
        for (size_type i = 0; i < N; ++i){
            init_func(slots_[i].data, i);
        }
    }

    void initialize(std::function<void(T&)> init_func) {
        std::lock_guard<std::mutex> lk(mutex_);
        for (size_type i = 0; i < N; ++i) {
            init_func(slots_[i].data);
        }
    }

    // ================================================================ 借出

    /**
     * @brief 借出一个空闲 Slot（阻塞版）
     *
     * 若池中无空闲 Slot，调用线程阻塞直到有 Slot 被归还。
     * 返回的 Slot 初始 ref_count = 1，调用者是第一个持有者。
     *
     * @return 非 nullptr 的 Slot 指针
     */
    Slot* acquire() {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait(lk, [this]{ return free_count_ > 0; });
        return acquire_impl();
    }

    /**
     * @brief 借出一个空闲 Slot（非阻塞版）
     *
     * @return Slot 指针；无空闲时返回 nullptr
     */
    Slot* try_acquire() {
        std::lock_guard<std::mutex> lk(mutex_);
        if (free_count_ == 0) return nullptr;
        return acquire_impl();
    }

    /**
     * @brief 借出一个空闲 Slot（超时版）
     *
     * @param timeout_ms 最长等待毫秒数
     * @return Slot 指针；超时返回 nullptr
     */
    Slot* acquire_for(unsigned int timeout_ms) {
        std::unique_lock<std::mutex> lk(mutex_);
        bool ok = cv_.wait_for(
            lk,
            std::chrono::milliseconds(timeout_ms),
            [this]{ return free_count_ > 0; }
        );
        if (!ok) return nullptr;
        return acquire_impl();
    }

    // ================================================================ 强制归还（忽略引用计数）

    /**
     * @brief 强制将 Slot 归还池（不检查 ref_count，用于异常清理）
     *
     * 正常流程应使用 slot->release() 自动归还。
     * 此接口用于"持有者已确定不会再访问该 Slot"的强制回收场景。
     *
     * @throws std::invalid_argument 若 slot 不属于本池或未处于借出状态
     */
    void force_release(Slot* slot) {
        if (!slot) return;
        validate_slot(slot);
        {
            std::lock_guard<std::mutex> lk(mutex_);
            if (!slot->in_use_) {
                throw std::invalid_argument(
                    "PoolBuffer::force_release: slot is not currently acquired");
            }
            slot->ref_count_.store(0, std::memory_order_relaxed);
            slot->in_use_ = false;
            ++free_count_;
        }
        cv_.notify_one();
    }

    // ================================================================ 状态查询

    size_type free_count() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return free_count_;
    }

    size_type used_count() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return N - free_count_;
    }

    bool exhausted() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return free_count_ == 0;
    }

    constexpr size_type capacity() const noexcept { return N; }

    // ================================================================ 外部锁接口
    void lock()         const { mutex_.lock();        }
    void unlock()       const { mutex_.unlock();      }
    bool try_lock()     const { return mutex_.try_lock(); }

private:
    // ---------------------------------------------------------------- 内部：引用归零时由 Slot::release() 回调
    void return_slot(Slot* slot) {
        {
            std::lock_guard<std::mutex> lk(mutex_);
            assert(slot->in_use_);
            slot->in_use_ = false;
            ++free_count_;
        }
        cv_.notify_one();
    }

    // ---------------------------------------------------------------- 内部：无锁借出（调用方已持锁且 free_count_ > 0）
    Slot* acquire_impl() {
        for (size_type i = 0; i < N; ++i) {
            if (!slots_[i].in_use_) {
                slots_[i].in_use_ = true;
//                slots_[i].ref_count_.store(1, std::memory_order_relaxed);
                --free_count_;
                return &slots_[i];
            }
        }
        assert(false && "acquire_impl: no free slot found despite free_count_ > 0");
        return nullptr;
    }

    // ---------------------------------------------------------------- 内部：校验 slot 归属
    void validate_slot(Slot* slot) const {
        // 检查指针是否在 slots_ 数组范围内且对齐
        const Slot* begin = slots_.data();
        const Slot* end   = begin + N;
        if (slot < begin || slot >= end) {
            throw std::invalid_argument(
                "PoolBuffer: slot does not belong to this pool");
        }
    }

    std::array<Slot, N>     slots_;       ///< 固定槽位数组
    size_type               free_count_;  ///< 空闲槽位计数
    mutable std::mutex      mutex_;       ///< 池级互斥锁
    std::condition_variable cv_;          ///< 空闲槽位通知
};
