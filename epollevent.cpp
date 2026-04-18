#include "epollevent.h"

#include "outLog.h"

EpollEvent::EpollEvent() {
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1) {
        throw std::runtime_error("epoll_create1 failed");
    }

    wake_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wake_fd_ == -1) {
        close(epoll_fd_);
        throw std::runtime_error("eventfd failed");
    }

    LOG_DEBUG("EpollEvent", epoll_fd_, wake_fd_);

    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = wake_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wake_fd_, &ev) == -1) {
        close(wake_fd_);
        close(epoll_fd_);
        LOG_DEBUG("epoll", "epoll_ctl add wake_fd failed");
    }
}

EpollEvent::~EpollEvent() {
    stop();
    if (wake_fd_ != -1) close(wake_fd_);
    if (epoll_fd_ != -1) close(epoll_fd_);
}

void EpollEvent::add_fd(int fd, Callback callback) {
    if (fd < 0 || !callback) return;

    setNonBlock(fd);

    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        std::cerr << "epoll_ctl ADD failed, fd=" << fd << ", errno=" << errno << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(mtx_);
    callbacks_[fd] = std::make_shared<Callback>(std::move(callback));;
}
void EpollEvent::erase(int fd){
     std::lock_guard<std::mutex> lock(mtx_);
     auto it = callbacks_.find(fd);
     if (it != callbacks_.end()) {
         epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
         callbacks_.erase(fd);
         return;
     }
}
void EpollEvent::start(int timeout_ms){
    thread_.set_loop_callback([this, timeout_ms](SafeThread* self)->bool{
        (void)self;
        int nfds = epoll_wait(epoll_fd_, events.data(), events.size(), timeout_ms);
        if (nfds == -1) {
            if (errno == EINTR) return true;
            std::cerr << "epoll_wait error: " << errno << std::endl;
            return false;
        }else if(nfds==0){
             std::lock_guard<std::mutex> lock(mtx_);
             for(auto& callback : callbacks_){
                 (*callback.second)(0, 0, Message::Timeout);
             }
            return true;
        }

        for (int i = 0; i < nfds; ++i){
            int fd = events[i].data.fd;
            uint32_t e = events[i].events;

            if (fd == wake_fd_) {
                uint64_t val;
                while (read(wake_fd_, &val, sizeof(val)) == sizeof(val)) {}
                continue;
            }

            std::shared_ptr<Callback> cb_ptr;
            {
                std::lock_guard<std::mutex> lock(mtx_);
                auto it = callbacks_.find(fd);
                if (it != callbacks_.end()) {
                    cb_ptr = it->second; // 只拷贝智能指针，增加引用计数，保证回调执行时安全
                }
            }
            if (cb_ptr) {
                (*cb_ptr)(fd, e, Message::Data);
            }
            if (e & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    for(auto& callback : callbacks_){
                        (*callback.second)(0, 0, Message::Error);
                    }
                }
                erase(fd);
            }
        }

        return true;
    });
    thread_.start("EpollEvent");
}

void EpollEvent::stop(){
    uint64_t one = 1;
    size_t n = write(wake_fd_, &one, sizeof(one));
    (void)n;
    thread_.stop();
}

void EpollEvent::setNonBlock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
