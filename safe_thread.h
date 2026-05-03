#pragma once
#include <atomic>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

class SafeThread {
public:
    using ThreadCallback = std::function<bool(SafeThread*)>;

private:
    enum class State { Stopped, Starting, Running, Stopping };

    std::mutex mtx_;
    std::condition_variable cv_;
    State state_{State::Stopped};       // 唯一状态源，mtx_ 保护
    std::thread thread_;

    ThreadCallback start_callback_;
    ThreadCallback end_callback_;
    ThreadCallback loop_callback_;
    std::string thread_name_;
    std::atomic<bool> quit_{false};

    void eventLoop();

public:
    void start(const std::string& name = "default");
    void stop();

    bool isRunning() ;

    void set_start_callback(ThreadCallback cb);
    void set_end_callback(ThreadCallback cb);
    void set_loop_callback(ThreadCallback cb);

    static void nsDelay(int ns);
    static void usDelay(int us);
    static void msDelay(int ms);
    static void sDelay(int s);

    SafeThread() = default;
    ~SafeThread() { stop(); }

    // 禁止拷贝和移动
    SafeThread(const SafeThread&) = delete;
    SafeThread& operator=(const SafeThread&) = delete;
};
