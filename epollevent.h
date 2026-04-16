#pragma once

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <functional>
#include <unordered_map>
#include <vector>
#include <cerrno>
#include <iostream>
#include <memory>
#include <array>

#include "safe_thread.h"

class EpollEvent {
public:
    EpollEvent();
    virtual ~EpollEvent();

    enum class Message{
        Data, Timeout,Error
    };

    using Callback = std::function<void(int, uint32_t, Message)>;
    void add_fd(int fd, Callback callback);
    void erase(int fd);

    void start(int timeout_ms=2000);
    void stop();

    static void setNonBlock(int fd);
private:
    SafeThread thread_;
    std::mutex mtx_;

    int epoll_fd_ = -1;
    int wake_fd_ = -1;
    std::unordered_map<int, std::shared_ptr<Callback>> callbacks_;

    std::array<epoll_event, 16> events;
};
