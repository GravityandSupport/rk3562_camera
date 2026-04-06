#ifndef REF_ARRAY_H
#define REF_ARRAY_H

#include <vector>
#include <mutex>
#include <functional>

class RefArray {
public:
    struct Item {
        int ref_count = 0;
    };

private:
    std::vector<Item> items_;
    std::mutex mtx_;
    std::function<void(int)> on_zero_;

public:
    // 设置容量
    void setSize(size_t size);

    // 设置回调函数
    void setOnZeroBackCall(std::function<void(int)> cb);

    // 增加引用计数
    bool acquire(int index);

    // 释放引用计数
    void release(int index);
};

#endif // REF_ARRAY_H
