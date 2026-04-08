// ====================== TimerManager.cpp ======================
#include "timermanager.h"
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>  // for strerror

TimerManager::TimerManager() {
    next_id_ = 1;
    epoll_fd_ = epoll_create1(0);
    stop_event_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    running_.store(false, std::memory_order_relaxed);
    detached_.store(false, std::memory_order_relaxed);

    if (epoll_fd_ == -1 || stop_event_fd_ == -1) {
        std::cerr << "TimerManager 初始化失败: " << strerror(errno) << std::endl;
        return;
    }

    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = stop_event_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, stop_event_fd_, &ev);
}

TimerManager::~TimerManager() {
    stop();
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        for (auto& p : timers_) {
            close(p.second.fd);
        }
        timers_.clear();
        fd_to_id_.clear();
    }
    if (epoll_fd_ != -1) close(epoll_fd_);
    if (stop_event_fd_ != -1) close(stop_event_fd_);
}

TimerManager::TimerId TimerManager::createTimer(
    std::chrono::nanoseconds first_fire_delay,
    std::chrono::nanoseconds interval,
    Callback callback) {

    if (first_fire_delay < std::chrono::nanoseconds::zero()) {
        first_fire_delay = std::chrono::nanoseconds::zero();
    }

    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd == -1) {
        std::cerr << "timerfd_create 失败: " << strerror(errno) << std::endl;
        return 0;
    }

    TimerId id = next_id_.fetch_add(1, std::memory_order_relaxed);

    TimerInfo info{};
    info.fd = fd;
    info.callback = std::move(callback);
    info.first_delay = first_fire_delay;
    info.interval = interval;
    info.paused = false;
    info.remaining = std::chrono::nanoseconds::zero();

    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        timers_[id] = std::move(info);
        fd_to_id_[fd] = id;
    }

    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        std::cerr << "epoll_ctl ADD 失败: " << strerror(errno) << std::endl;
    }

    return id;
}

void TimerManager::start(bool detach_thread) {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;

    worker_thread_ = std::thread(&TimerManager::eventLoop, this);
    detached_.store(detach_thread, std::memory_order_relaxed);

    if (detach_thread) {
        worker_thread_.detach();
    }
}

void TimerManager::modifyTimer(TimerId id,
                               std::chrono::nanoseconds new_first_fire_delay,
                               std::chrono::nanoseconds new_interval) {
    if (new_first_fire_delay < std::chrono::nanoseconds::zero()) {
        new_first_fire_delay = std::chrono::nanoseconds::zero();
    }

    std::lock_guard<std::mutex> lock(map_mutex_);
    auto it = timers_.find(id);
    if (it == timers_.end()) return;

    auto& info = it->second;
    info.first_delay = new_first_fire_delay;
    info.interval = new_interval;

    struct itimerspec zero{};
    timerfd_settime(info.fd, 0, &zero, nullptr);

    if (!info.paused) {
        struct itimerspec its{};
        its.it_value = durationToTimespec(info.first_delay);
        its.it_interval = durationToTimespec(info.interval);
        if (timerfd_settime(info.fd, 0, &its, nullptr) == -1) {
            std::cerr << "modifyTimer timerfd_settime 失败: " << strerror(errno) << std::endl;
        }
    } else {
        info.remaining = new_first_fire_delay;
    }
}

void TimerManager::startTimer(TimerId id) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto it = timers_.find(id);
    if (it == timers_.end()) return;

    auto& info = it->second;
    info.paused = false;
    info.remaining = std::chrono::nanoseconds::zero();

    struct itimerspec its{};
    its.it_value = durationToTimespec(info.first_delay);
    its.it_interval = durationToTimespec(info.interval);
    timerfd_settime(info.fd, 0, &its, nullptr);
}

void TimerManager::pauseTimer(TimerId id) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto it = timers_.find(id);
    if (it == timers_.end() || it->second.paused) return;

    auto& info = it->second;
    struct itimerspec curr{};
    if (timerfd_gettime(info.fd, &curr) == -1) return;

    struct itimerspec zero{};
    timerfd_settime(info.fd, 0, &zero, nullptr);

    info.remaining = timespecToDuration(curr.it_value);
    info.paused = true;
}

void TimerManager::resumeTimer(TimerId id) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto it = timers_.find(id);
    if (it == timers_.end() || !it->second.paused) return;

    auto& info = it->second;
    struct itimerspec its{};
    its.it_value = durationToTimespec(info.remaining);
    its.it_interval = durationToTimespec(info.interval);
    timerfd_settime(info.fd, 0, &its, nullptr);

    info.paused = false;
    info.remaining = std::chrono::nanoseconds::zero();
}

void TimerManager::stopTimer(TimerId id) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto it = timers_.find(id);
    if (it == timers_.end()) return;

    auto& info = it->second;
    struct itimerspec zero{};
    timerfd_settime(info.fd, 0, &zero, nullptr);

    info.paused = false;
    info.remaining = std::chrono::nanoseconds::zero();
}

void TimerManager::destroyTimer(TimerId id) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto it = timers_.find(id);
    if (it == timers_.end()) return;

    int fd = it->second.fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    fd_to_id_.erase(fd);
    timers_.erase(it);
}

void TimerManager::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) return;

    uint64_t one = 1;
    write(stop_event_fd_, &one, sizeof(one));

    if (!detached_.load(std::memory_order_relaxed) && worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

// ====================== 私有辅助函数实现 ======================
struct timespec TimerManager::durationToTimespec(std::chrono::nanoseconds d) {
    if (d <= std::chrono::nanoseconds::zero()) return {0, 0};
    auto ns = d.count();
    long long secs = ns / 1000000000LL;
    long long nsecs = ns % 1000000000LL;
    return {static_cast<time_t>(secs), static_cast<long>(nsecs)};
}

std::chrono::nanoseconds TimerManager::timespecToDuration(const struct timespec& ts) {
    if (ts.tv_sec < 0 || ts.tv_nsec < 0) return std::chrono::nanoseconds::zero();
    return std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec);
}

void TimerManager::eventLoop() {
    while (running_.load(std::memory_order_relaxed)) {
        struct epoll_event events[32];
        int nfds = epoll_wait(epoll_fd_, events, 32, -1);

        if (nfds == -1) {
            if (errno == EINTR) continue;
            std::cerr << "epoll_wait 错误: " << strerror(errno) << std::endl;
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (fd == stop_event_fd_) {
                uint64_t dummy;
                read(stop_event_fd_, &dummy, sizeof(dummy));
                continue;
            }

            TimerId tid = 0;
            Callback cb_copy;
            uint64_t exp_count = 0;
            bool valid = false;

            {
                std::lock_guard<std::mutex> lock(map_mutex_);
                auto fit = fd_to_id_.find(fd);
                if (fit == fd_to_id_.end()) continue;
                tid = fit->second;
                auto tit = timers_.find(tid);
                if (tit == timers_.end()) continue;

                ssize_t nread = read(fd, &exp_count, sizeof(exp_count));
                if (nread == sizeof(uint64_t) && exp_count > 0) {
                    cb_copy = tit->second.callback;
                    valid = true;
                }
            }

            if (valid && exp_count > 0) {
                for (uint64_t j = 0; j < exp_count; ++j) {
                    if (!running_.load(std::memory_order_relaxed)) break;
                    cb_copy(tid);
                }
            }
        }
    }
}