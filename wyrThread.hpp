#pragma once

#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <atomic>


class __Thread_t  : public std::enable_shared_from_this<__Thread_t> {
public:
    enum class threadMode {detach, join};
    enum class joinMode {BLOCK, NOBLOCK};

    threadMode mode;

    __Thread_t() = default;
    virtual ~__Thread_t() = default;

    // 🔥 禁止拷贝和赋值（极其重要）
    __Thread_t(const __Thread_t&) = delete;
    __Thread_t& operator=(const __Thread_t&) = delete;

    // 🔥 也禁止移动（更安全）
    __Thread_t(__Thread_t&&) = delete;
    __Thread_t& operator=(__Thread_t&&) = delete;

    virtual bool run(const char* __name, threadMode __mode = threadMode::detach);
    void join();

    virtual bool startCallback();
    virtual bool endCallback();
    virtual bool threadLoop() = 0;
    int requestExitAndWait(joinMode __mode=joinMode::BLOCK, int time_out=3000); // ms until
    bool isRunning();

    void resume();
    void suspend();
    bool isSuspend();

    const std::string& getName() {return name;}
private:
    static bool _threadLoop(__Thread_t* self);

    volatile bool mRunning = false;
    volatile bool mExitPending = false;
    std::shared_ptr<__Thread_t> mHoldSelf;
    std::thread mThread;
    std::mutex mMutex;
    std::condition_variable cv;

    bool m_suspend = false;          // 是否挂起
    std::condition_variable m_suspend_cv;        // 挂起条件变量

    std::string name;
};

