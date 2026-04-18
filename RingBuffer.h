#pragma once

#include <array>
#include <mutex>
#include <cstddef>
#include <stdexcept>

/**
 * @brief 环形缓冲区模板类
 *
 * @tparam T  元素类型
 * @tparam N  缓冲区槽位数量（编译期常量，固定不变）
 *
 * 设计语义
 * --------
 *  - 缓冲区拥有 N 个固定槽位，存储在 std::array<T, N> 中，不增不减。
 *  - 内部维护一个"当前迭代器"（cursor），指向某个槽位。
 *  - current() 返回当前槽位元素的指针，调用者可直接读写该槽位内容。
 *  - next()    将迭代器向后推进一格；若已到达最后一个槽位，自动回绕到 0。
 *  - 迭代器的推进时机完全由调用者决定，类本身不自动移动。
 *
 * 线程安全
 * --------
 *  - lock() / unlock() / try_lock() 暴露内部互斥锁，供外部在需要
 *    连续操作（如"推进 + 写入"）时保证原子性。
 *  - next() 和 current() 自身不加锁，调用者按需持锁后调用。
 *
 * 典型用法
 * --------
 * @code
 *   RingBuffer<MyData, 8> rb;
 *
 *   // 写入端（生产者）：推进到下一槽位，直接写入
 *   rb.lock();
 *   rb.next();
 *   MyData* slot = rb.current();
 *   slot->field = value;
 *   rb.unlock();
 *
 *   // 读取端（消费者）：读取当前槽位
 *   rb.lock();
 *   const MyData* slot = rb.current();
 *   process(*slot);
 *   rb.unlock();
 * @endcode
 */
template <typename T, std::size_t N>
class RingBuffer {
    static_assert(N > 0, "RingBuffer: capacity N must be > 0");

public:
    using value_type    = T;
    using size_type     = std::size_t;
    using pointer       = T*;
    using const_pointer = const T*;

    // ---------------------------------------------------------------- 构造
    RingBuffer() : cursor_(0) {}

    // mutex 不可复制/移动，故整个类也禁止
    RingBuffer(const RingBuffer&)            = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&)                 = delete;
    RingBuffer& operator=(RingBuffer&&)      = delete;

    // ---------------------------------------------------------------- 迭代器控制

    /**
     * @brief 将内部迭代器推进一格
     *
     * 若当前已在最后一个槽位（N-1），则自动回绕到槽位 0。
     * 何时推进由调用者决定；若有并发访问请先持锁。
     */
    void next() noexcept {
        cursor_ = (cursor_ + 1) % N;
    }

    /**
     * @brief 将内部迭代器回退一格
     *
     * 若当前已在槽位 0，则回绕到最后一个槽位（N-1）。
     */
    void prev() noexcept {
        cursor_ = (cursor_ + N - 1) % N;
    }

    /**
     * @brief 将迭代器跳转到指定物理槽位
     *
     * @throws std::out_of_range 若 index >= N
     */
    void seek(size_type index) {
        if (index >= N) {
            throw std::out_of_range("RingBuffer::seek: index out of range");
        }
        cursor_ = index;
    }

    /**
     * @brief 将迭代器重置到槽位 0
     */
    void reset() noexcept {
        cursor_ = 0;
    }

    // ---------------------------------------------------------------- 元素访问

    /**
     * @brief 返回当前迭代器所指槽位的元素指针
     *
     * 调用者通过该指针直接读写槽位内容，无需经过 push/pop。
     * 若有并发访问，调用前须先持锁。
     *
     * @return 指向当前槽位元素的指针，永不为 nullptr
     */
    pointer current() noexcept {
        return &buffer_[cursor_];
    }

    const_pointer current() const noexcept {
        return &buffer_[cursor_];
    }

    /**
     * @brief 查看距当前迭代器偏移 offset 格的槽位指针（不移动迭代器）
     *
     * offset 支持正负值，自动取模回绕。
     * 例如 offset=1 相当于"下一个"，offset=-1 相当于"上一个"。
     */
    pointer peek_offset(int offset) noexcept {
        int idx = (static_cast<int>(cursor_) + offset % static_cast<int>(N)
                   + static_cast<int>(N)) % static_cast<int>(N);
        return &buffer_[static_cast<size_type>(idx)];
    }

    const_pointer peek_offset(int offset) const noexcept {
        int idx = (static_cast<int>(cursor_) + offset % static_cast<int>(N)
                   + static_cast<int>(N)) % static_cast<int>(N);
        return &buffer_[static_cast<size_type>(idx)];
    }

    /**
     * @brief 按物理槽位下标直接访问（不影响迭代器位置）
     *
     * @throws std::out_of_range 若 index >= N
     */
    pointer at(size_type index) {
        if (index >= N) throw std::out_of_range("RingBuffer::at: index out of range");
        return &buffer_[index];
    }

    const_pointer at(size_type index) const {
        if (index >= N) throw std::out_of_range("RingBuffer::at: index out of range");
        return &buffer_[index];
    }

    // ---------------------------------------------------------------- 状态查询

    /** 当前迭代器所在的物理槽位下标 */
    size_type cursor_index() const noexcept { return cursor_; }

    /** 缓冲区固定槽位总数（即模板参数 N） */
    constexpr size_type capacity() const noexcept { return N; }

    /** 返回底层数组首元素指针（物理连续存储） */
    pointer       data()       noexcept { return buffer_.data(); }
    const_pointer data() const noexcept { return buffer_.data(); }

    // ---------------------------------------------------------------- 线程安全接口

    /**
     * @brief 加锁（阻塞等待）
     *
     * 在需要"推进迭代器 + 访问槽位"原子执行时，在操作前调用。
     */
    void lock()   const { mutex_.lock();   }

    /**
     * @brief 解锁，与 lock() 配对使用
     */
    void unlock() const { mutex_.unlock(); }

    /**
     * @brief 尝试加锁（非阻塞）
     * @return true 成功；false 锁已被其他线程持有
     */
    bool try_lock() const { return mutex_.try_lock(); }

private:
    std::array<T, N>   buffer_;   ///< 固定槽位存储
    size_type          cursor_;   ///< 当前迭代器位置（物理下标）
    mutable std::mutex mutex_;    ///< 互斥锁（mutable 允许 const 成员加锁）
};
