#include "tcpclient.h"
#include "outLog.h"

void TcpClient::connect(const std::string& ip, uint16_t port){
    createSocket();
    setNonBlock(fd_);

    sockaddr_in addr = makeAddr(ip, port);
    ::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    LOG_DEBUG("tcp client", fd_);
    epoll_event.add_fd(fd_, [&](int fd, uint32_t events, EpollEvent::Message message){
        if(message==EpollEvent::Message::Data){
            (void)fd;

            LOG_DEBUG("tcp client", events);
            if(events & (EPOLLERR | EPOLLHUP)){
                state_ = State::Closed;
                if(tcp_device){
                    tcp_device->onDisconnect();
                }
                LOG_DEBUG("tcp client", "服务端关闭了连接 (FIN)");
                return;
            }

            if(events & EPOLLOUT){
                int err = 0;
                socklen_t len = sizeof(err);
                ::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &len);
                if (err == 0){
                    if(state_!=State::Connected){
                        state_ = State::Connected;
                        LOG_DEBUG("tcp client", "tcp连接服务端成功");
                        if(tcp_device){
                            tcp_device->fd = fd_;
                            tcp_device->onConnect();
                        }
                    }
                }else {
                    // 打印具体的错误原因，比如 Connection refused (ECONNREFUSED)
                    LOG_ERRORC("tcp client", "tcp连接失败, 错误原因: %s", strerror(err));
                }
            }

            if(events&EPOLLIN && state_==State::Connected){ // 收到服务端数据
                char buffer[1024];
                ssize_t n = read(fd, buffer, sizeof(buffer));

                if (n > 0){// 收到服务端数据

                }else if(n==0){ // 服务端主动断开
                    LOG_DEBUG("tcp client", "服务端关闭了连接 (FIN)");
                }else {
                    if (errno != EAGAIN) {
                        LOG_ERRORC("Network", "读取出错: %s", strerror(errno));
//                        close(fd);
                    }
                }
            }
        }
    });
    epoll_event.start(2000);
}















void TcpClient::setNonBlock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}


void TcpClient::createSocket() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    state_ = State::Closed;
}
bool TcpClient::checkFd() const{
    return fd_<0;
}
bool TcpClient::checkConnected() const {
    if (state_ != State::Connected)
        return false;
    return true;
}

sockaddr_in TcpClient::makeAddr(const std::string& ip, uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0)
        throw std::runtime_error("Invalid IP: " + ip);
    return addr;
}
