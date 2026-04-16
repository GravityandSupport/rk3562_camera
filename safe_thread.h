#pragma once

#include <atomic>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

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

    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> started_{false};
    std::atomic<bool> quit_{false};

    std::string thread_name_;
public:
    void start(const std::string& name_="default");
    void stop();

    static void nsDelay(int ns);
    static void usDelay(int us);
    static void msDelay(int ms);
    static void sDelay(int s);

    void set_start_callback(ThreadCallback cb);
    void set_end_callback(ThreadCallback cb);
    void set_loop_callback(ThreadCallback cb);

    SafeThread(/* args */);
    virtual ~SafeThread();
};
