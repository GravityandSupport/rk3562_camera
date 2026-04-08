// ====================== TimerManager.h ======================
#pragma once

#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <vector>

class TimerManager {
public:
    using TimerId = uint64_t;
    using Callback = std::function<void(TimerId)>;   // 回调带 TimerId

    TimerManager();
    ~TimerManager();

    // 创建定时器（不立即启动）
    TimerId createTimer(std::chrono::nanoseconds first_fire_delay,
                        std::chrono::nanoseconds interval,
                        Callback callback);

    // 启动后台 epoll 线程
    // detach_thread = false（默认）：析构时自动 join，适合局部变量
    // detach_thread = true：线程 detached，适合全局/堆对象
    void start(bool detach_thread = false);

    // 修改定时器参数（立即生效）
    void modifyTimer(TimerId id,
                     std::chrono::nanoseconds new_first_fire_delay,
                     std::chrono::nanoseconds new_interval);

    void startTimer(TimerId id);
    void pauseTimer(TimerId id);
    void resumeTimer(TimerId id);
    void stopTimer(TimerId id);
    void destroyTimer(TimerId id);

    // 停止后台线程（无论 detach 与否都会尝试停止）
    void stop();

private:
    struct TimerInfo {
        int fd = -1;
        Callback callback;
        std::chrono::nanoseconds first_delay{0};
        std::chrono::nanoseconds interval{0};
        bool paused = false;
        std::chrono::nanoseconds remaining{0};
    };

    // 辅助函数声明（在 .cpp 中实现）
    static struct timespec durationToTimespec(std::chrono::nanoseconds d);
    static std::chrono::nanoseconds timespecToDuration(const struct timespec& ts);

    void eventLoop();

    std::atomic<uint64_t> next_id_;
    int epoll_fd_ = -1;
    int stop_event_fd_ = -1;
    std::atomic<bool> running_;
    std::atomic<bool> detached_;
    std::thread worker_thread_;

    std::unordered_map<TimerId, TimerInfo> timers_;
    std::unordered_map<int, TimerId> fd_to_id_;
    std::mutex map_mutex_;
};

/*
                            示例程序 
#include "timermanager.h"
#include <iostream>
#include <thread>

int main() {
    TimerManager tm;
    tm.start();   // 默认模式，适合局部变量

    auto id = tm.createTimer(std::chrono::seconds(1),
                             std::chrono::seconds(1),
                             [](TimerManager::TimerId i){ std::cout << "timer " << i << " fired\n"; });
    tm.startTimer(id);

    std::this_thread::sleep_for(std::chrono::seconds(5));
    tm.stop();
    return 0;
}
*/