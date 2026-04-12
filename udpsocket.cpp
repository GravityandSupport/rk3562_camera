#include "udpsocket.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <vector>

#include "outLog.h"
#include "safe_thread.h"

constexpr int MAX_EVENTS = 64;

// ==================== 内部实现类 ====================
class UdpSocket::Impl {
public:
    Impl() : sockfd_(-1), epollfd_(-1){}
    ~Impl() { stop(); }

    bool create();
    bool bind(const std::string& ip, uint16_t port);
    bool setNonBlocking();
    ssize_t sendTo(const std::string& ip, uint16_t port, const void* data, size_t len);

    void registerCallback(const std::string& remote_ip, uint16_t remote_port, RecvCallback callback);
    void removeCallback(const std::string& remote_ip, uint16_t remote_port);
    void removeAllCallbacksForIp(const std::string& remote_ip);

    bool start();
    void stop();
    int fd() const { return sockfd_; }

private:
    void eventLoop(size_t buffer_size);
    void handleRecv();

    // 使用 "IP:Port" 作为 key 的精确匹配
    std::string makeKey(const std::string& ip, uint16_t port) const {
        return ip + ":" + std::to_string(port);
    }

private:
    int sockfd_;
    int epollfd_;
    std::unordered_map<std::string, RecvCallback> callbacks_;

    SafeThread thread_;

    std::vector<uint8_t> buffer_;
};

// ==================== UdpSocket 公共接口实现 ====================

UdpSocket::UdpSocket() : pimpl_(std::unique_ptr<Impl>(new Impl())) {}
UdpSocket::~UdpSocket(){
    stop();
};

bool UdpSocket::create()                    { return pimpl_->create(); }
bool UdpSocket::bind(const std::string& ip, uint16_t port) { return pimpl_->bind(ip, port); }
bool UdpSocket::setNonBlocking()            { return pimpl_->setNonBlocking(); }
ssize_t UdpSocket::sendTo(const std::string& ip, uint16_t port,
                          const void* data, size_t len) {
    return pimpl_->sendTo(ip, port, data, len);
}

void UdpSocket::registerCallback(const std::string& remote_ip, uint16_t remote_port,
                                 RecvCallback callback) {
    pimpl_->registerCallback(remote_ip, remote_port, std::move(callback));
}

void UdpSocket::removeCallback(const std::string& remote_ip, uint16_t remote_port) {
    pimpl_->removeCallback(remote_ip, remote_port);
}

void UdpSocket::removeAllCallbacksForIp(const std::string& remote_ip) {
    pimpl_->removeAllCallbacksForIp(remote_ip);
}

bool UdpSocket::start() { return pimpl_->start(); }
void UdpSocket::stop() { pimpl_->stop(); }
int UdpSocket::fd() const { return pimpl_->fd(); }

// ==================== Impl 具体实现 ====================

bool UdpSocket::Impl::create() {
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    return sockfd_ != -1;
}

bool UdpSocket::Impl::bind(const std::string& ip, uint16_t port) {
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if(ip.empty()){
        addr.sin_addr.s_addr = INADDR_ANY;
    }else{
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
    }

    return ::bind(sockfd_, (struct sockaddr*)&addr, sizeof(addr)) == 0;
}
bool UdpSocket::Impl::setNonBlocking() {
    int flags = fcntl(sockfd_, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK) == 0;
}
ssize_t UdpSocket::Impl::sendTo(const std::string& ip, uint16_t port,
                                const void* data, size_t len) {
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());

    return sendto(sockfd_, data, len, 0, (struct sockaddr*)&addr, sizeof(addr));
}
void UdpSocket::Impl::registerCallback(const std::string& remote_ip,
                                       uint16_t remote_port,
                                       RecvCallback callback) {
    std::string key = makeKey(remote_ip, remote_port);
    callbacks_[key] = std::move(callback);
}

void UdpSocket::Impl::removeCallback(const std::string& remote_ip, uint16_t remote_port) {
    callbacks_.erase(makeKey(remote_ip, remote_port));
}

void UdpSocket::Impl::removeAllCallbacksForIp(const std::string& remote_ip) {
    for (auto it = callbacks_.begin(); it != callbacks_.end(); ) {
        if (it->first.rfind(remote_ip + ":", 0) == 0) {
            it = callbacks_.erase(it);
        } else {
            ++it;
        }
    }
}
bool UdpSocket::Impl::start() {
    if (sockfd_ == -1) return false;

    epollfd_ = epoll_create1(0);
    if (epollfd_ == -1) return false;

    setNonBlocking();

    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;   // 边缘触发
    ev.data.fd = sockfd_;

    if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, sockfd_, &ev) == -1) {
        close(epollfd_);
        epollfd_ = -1;
        return false;
    }

    buffer_.resize(2048);

    thread_.start();
    thread_.set_loop_callback([this](SafeThread* self) ->bool{
        (void)self;
        struct epoll_event events[MAX_EVENTS];
        int nfds = epoll_wait(epollfd_, events, MAX_EVENTS, 100);  // 100ms 超时
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == sockfd_) {
                handleRecv();
            }
        }
        return true;
    });
    return true;
}
void UdpSocket::Impl::stop() {
    thread_.stop();
    if (epollfd_ != -1) {
        close(epollfd_);
        epollfd_ = -1;
    }
}
void UdpSocket::Impl::handleRecv() {
    while (true) {  // 边缘触发需一次性读空
        struct sockaddr_in sender_addr{};
        socklen_t addr_len = sizeof(sender_addr);

        ssize_t ret = recvfrom(sockfd_, buffer_.data(), buffer_.size(), 0,
                               (struct sockaddr*)&sender_addr, &addr_len);

        if (ret > 0) {
            std::string sender_ip = inet_ntoa(sender_addr.sin_addr);
            uint16_t sender_port = ntohs(sender_addr.sin_port);

            std::string key = makeKey(sender_ip, sender_port);

//            LOG_DEBUG("UDP SOCKET", sender_ip, sender_port);
            auto it = callbacks_.find(key);
            if (it != callbacks_.end() && it->second) {
                it->second(reinterpret_cast<char*>(buffer_.data()), static_cast<size_t>(ret), sender_ip, sender_port);
            }
        }
        else if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;   // 数据读完
            }
            break;       // 其他错误
        }
        else {
            break;
        }
    }
}
