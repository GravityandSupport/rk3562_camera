#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <functional>
#include <cstring>
#include <cerrno>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "epollevent.h"
#include "tcp_device.h"

class TcpClient
{
public:
    enum class State { Closed, Connecting, Connected };

    TcpDevice* tcp_device=nullptr;

    TcpClient() = default;
    ~TcpClient() { /*close(); */}

    TcpClient(const TcpClient&)            = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    void connect(const std::string& ip, uint16_t port);

private:
    int   fd_    {-1};
    State state_ {State::Closed};

    void createSocket();

    bool checkFd() const;
    bool checkConnected() const;
    static sockaddr_in makeAddr(const std::string& ip, uint16_t port);

    static void setNonBlock(int fd);

    EpollEvent epoll_event;
};

