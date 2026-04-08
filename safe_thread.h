#pragma once

#include <atomic>
#include <thread>
#include <functional>

class SafeThread
{
public:
    using ThreadCallback = std::function<bool(SafeThread*)>;
private:
    std::atomic<bool> running_{false};
    std::thread thread_;

    void eventLoop();

    ThreadCallback start_callback;
    ThreadCallback end_callback;
    ThreadCallback loop_callback;
public:
    void start();
    void stop();

    void set_start_callback(ThreadCallback cb);
    void set_end_callback(ThreadCallback cb);
    void set_loop_callback(ThreadCallback cb);

    SafeThread(/* args */);
    virtual ~SafeThread();
};

