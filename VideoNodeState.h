#ifndef VIDEONODESTATE_H
#define VIDEONODESTATE_H

#include "atomic"

class VideoNodeState {
public:
    /**
     * @brief 增加引用计数（使能）
     * 使用 fetch_add 确保原子递增
     */
    void enable() {
        ref_count.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief 减少引用计数（禁能）
     * 需注意防止计数器溢出为负数（取决于业务逻辑）
     */
    void disable() {
        // 只有当前计数大于 0 时才减少，防止误操作导致计数混乱
        int current = ref_count.load(std::memory_order_relaxed);
        while (current > 0 && !ref_count.compare_exchange_weak(current, current - 1, std::memory_order_relaxed));
    }

    /**
     * @brief 获取当前节点是否处于使能状态
     * @return true 表示引用计数 > 0，false 表示引用计数 <= 0
     */
    bool isEnabled() const {
        return ref_count.load(std::memory_order_relaxed) > 0;
    }

    /**
     * @brief 获取当前的引用计数值（通常用于调试）
     */
    int getRefCount() const {
        return ref_count.load(std::memory_order_relaxed);
    }

protected:
    // 初始化为 0，表示初始状态为禁能
    std::atomic<int> ref_count{0};
};

#endif // VIDEONODESTATE_H
