#include "safe_thread.h"
#include <chrono>
#include <pthread.h>
#include "outLog.h"

void SafeThread::start(const std::string& name) {
    std::unique_lock<std::mutex> lock(mtx_);

    if (state_ != State::Stopped) {
        // Running / Starting / Stopping 都直接跳过
        LOG_DEBUG(name, "线程未停止，忽略 start()，当前状态:", (int)state_);
        return;
    }

    thread_name_ = name;
    state_ = State::Starting;
    quit_.store(false, std::memory_order_relaxed);

    // thread_ 的赋值在锁内，不会与其他 start() 并发
    thread_ = std::thread(&SafeThread::eventLoop, this);

    // 等待 eventLoop 完成初始化（进入 Running 状态）
    cv_.wait(lock, [this] { return state_ != State::Starting; });

    LOG_DEBUG(thread_name_, "线程启动完成");
}

void SafeThread::stop() {
    std::unique_lock<std::mutex> lock(mtx_);

    if (state_ != State::Running) {
        LOG_DEBUG(thread_name_, "线程未运行，忽略 stop()");
        return;
    }

    state_ = State::Stopping;
    quit_.store(true, std::memory_order_release);

    // 取出 thread_ 后解锁再 join，避免死锁
    // （eventLoop 结束时会 lock(mtx_) 修改 state_）
    std::thread t = std::move(thread_);
    lock.unlock();

    LOG_DEBUG(thread_name_, "等待线程退出...");
    if (t.joinable()) t.join();
    LOG_DEBUG(thread_name_, "线程已退出");
}

void SafeThread::eventLoop() {
    // 设置线程名（Linux）
    std::string tname = thread_name_.substr(0, 15);
    pthread_setname_np(pthread_self(), tname.c_str());

    // 通知 start() 我们已经进入 Running
    {
        std::lock_guard<std::mutex> lock(mtx_);
        state_ = State::Running;
    }
    cv_.notify_one();

    // 执行回调
    bool ok = true;
    if (start_callback_ && !start_callback_(this)) ok = false;

    if (ok) {
        while (!quit_.load(std::memory_order_acquire)) {
            if (loop_callback_ && !loop_callback_(this)) break;
        }
        if (end_callback_) end_callback_(this);
    }

    LOG_DEBUG(thread_name_, "eventLoop 结束");

    {
        std::lock_guard<std::mutex> lock(mtx_);
        state_ = State::Stopped;
    }
    cv_.notify_all();
}

void SafeThread::set_start_callback(ThreadCallback cb) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (state_ != State::Stopped) {
        LOG_DEBUG(thread_name_, "线程运行中，不允许设置回调");
        return;
    }
    start_callback_ = std::move(cb);
}

void SafeThread::set_end_callback(ThreadCallback cb) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (state_ != State::Stopped) { LOG_DEBUG(thread_name_, "线程运行中"); return; }
    end_callback_ = std::move(cb);
}

void SafeThread::set_loop_callback(ThreadCallback cb) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (state_ != State::Stopped) { LOG_DEBUG(thread_name_, "线程运行中"); return; }
    loop_callback_ = std::move(cb);
}

void SafeThread::nsDelay(int ns) { if (ns > 0) std::this_thread::sleep_for(std::chrono::nanoseconds(ns)); }
void SafeThread::usDelay(int us) { if (us > 0) std::this_thread::sleep_for(std::chrono::microseconds(us)); }
void SafeThread::msDelay(int ms) { if (ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
void SafeThread::sDelay(int s)   { if (s  > 0) std::this_thread::sleep_for(std::chrono::seconds(s)); }
