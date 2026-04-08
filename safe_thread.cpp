#include "safe_thread.h"
#include <utility> // 包含 std::move

SafeThread::SafeThread(/* args */){

}

SafeThread::~SafeThread(){
    stop();
}

void SafeThread::start(){
    if (running_.load(std::memory_order_relaxed)==true) return;

    thread_ = std::thread(&SafeThread::eventLoop, this);
}

void SafeThread::stop(){
    if (running_.load(std::memory_order_relaxed)==false) return;

    running_.store(false, std::memory_order_relaxed);
}

void SafeThread::eventLoop(){
    if(start_callback) {if(start_callback(this)==false) return;}
    running_.store(true, std::memory_order_relaxed);
    while (running_.load(std::memory_order_relaxed)){
        if(loop_callback){if(loop_callback(this)==false) return;}
    }
    if(end_callback) {end_callback(this);} 
    running_.store(false, std::memory_order_relaxed);
}


void SafeThread::set_start_callback(ThreadCallback cb) {
    start_callback = std::move(cb);
}

void SafeThread::set_end_callback(ThreadCallback cb) {
    end_callback = std::move(cb);
}

void SafeThread::set_loop_callback(ThreadCallback cb) {
    loop_callback = std::move(cb);
}
