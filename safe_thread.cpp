#include "safe_thread.h"
#include <utility> // 包含 std::move
#include <chrono>
#include "outLog.h"

SafeThread::SafeThread(/* args */){

}

SafeThread::~SafeThread(){
    stop(); // 最多超时 5000ms
}

void SafeThread::start(const std::string& name_){
    if (running_.load(std::memory_order_relaxed)==true) {
        LOG_DEBUG(thread_name_, "线程已经启动 你前后调用太快了");
        return;
    }

    started_ = false;

    thread_name_ = name_;
    thread_ = std::thread(&SafeThread::eventLoop, this);

    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this]{
        return started_.load();
    });

    LOG_DEBUG(thread_name_, "线程启动");
}

void SafeThread::stop(){
    if (running_.load(std::memory_order_relaxed)==false) {
        LOG_DEBUG(thread_name_, "线程没有启动 没有方法为你停止");
        return;
    }

    LOG_DEBUG(thread_name_, "线程阻塞等待退出中...");

    quit_.store(true, std::memory_order_relaxed);

    if (thread_.joinable()) {
        LOG_DEBUG(thread_name_ );
        thread_.join();          // epoll_wait 有超时 → join 一定会很快返回
    }

    LOG_DEBUG(thread_name_, "线程阻塞退出完成");
}

void SafeThread::eventLoop(){
    std::string thread_name = thread_name_.substr(0, 15); // 名字最长16个字符，包括'\0'，所以这里最多15个
    pthread_setname_np(pthread_self(), thread_name.c_str());

    running_.store(true, std::memory_order_relaxed);
    quit_.store(false, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(mtx_);
        started_ = true;
    }
    cv_.notify_one();

    bool flag = true;
    if(start_callback && start_callback(this)==false){
        flag = false;
    }
    if(flag){
        while (1){
            if(loop_callback){if(loop_callback(this)==false) break;}
            if(quit_.load(std::memory_order_relaxed)==true) {break;}
        }
        if(end_callback) {end_callback(this);}
    }

    LOG_DEBUG(thread_name_, "线程函数结束");
    running_.store(false, std::memory_order_relaxed);
}


void SafeThread::set_start_callback(ThreadCallback cb) {
    if (running_.load(std::memory_order_relaxed)==true) {
        LOG_DEBUG(thread_name_, "不允许在线程启动的情况下设置回调函数");
        return;
    }
    start_callback = std::move(cb);
}

void SafeThread::set_end_callback(ThreadCallback cb) {
    if (running_.load(std::memory_order_relaxed)==true) {
        LOG_DEBUG(thread_name_, "不允许在线程启动的情况下设置回调函数");
        return;
    }
    end_callback = std::move(cb);
}

void SafeThread::set_loop_callback(ThreadCallback cb) {
    if (running_.load(std::memory_order_relaxed)==true) {
        LOG_DEBUG(thread_name_, "不允许在线程启动的情况下设置回调函数");
        return;
    }
    loop_callback = std::move(cb);
}


// 纳秒延时
void SafeThread::nsDelay(int ns){
    if (ns <= 0) return;
    std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
}

// 微秒延时
void SafeThread::usDelay(int us){
    if (us <= 0) return;
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

// 毫秒延时
void SafeThread::msDelay(int ms){
    if (ms <= 0) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// 秒延时
void SafeThread::sDelay(int s){
    if (s <= 0) return;
    std::this_thread::sleep_for(std::chrono::seconds(s));
}
