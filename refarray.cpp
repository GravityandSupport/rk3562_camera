#include "refarray.h"

void RefArray::setSize(size_t size) {
    std::lock_guard<std::mutex> lock(mtx_);
    items_.clear();
    items_.resize(size);
}

void RefArray::setOnZeroBackCall(std::function<void(int)> cb) {
    std::lock_guard<std::mutex> lock(mtx_);
    on_zero_ = cb;
}

bool RefArray::acquire(int index) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (index < 0 || index >= static_cast<int>(items_.size())) {
        return false;
    }

    items_[index].ref_count++;
    return true;
}

void RefArray::release(int index) {
    std::function<void(int)> callback_to_run = nullptr;

    {
        // 使用局部作用域锁，确保在调用回调前释放锁
        std::lock_guard<std::mutex> lock(mtx_);

        if (index < 0 || index >= static_cast<int>(items_.size())) {
            return;
        }

        Item& item = items_[index];
        if (item.ref_count <= 0) {
            return;
        }

        item.ref_count--;

        // 如果计数归零且有回调，拷贝函数对象并在锁外执行
        if (item.ref_count == 0 && on_zero_) {
            callback_to_run = on_zero_;
        }
    }

    // 在锁释放后执行回调，防止死锁
    if (callback_to_run) {
        callback_to_run(index);
    }
}
