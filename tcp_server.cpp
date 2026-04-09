#include "tcp_server.h"

DeviceTypeID stringToDeviceType(const std::string& str) {
    static const std::vector<std::pair<std::string, DeviceTypeID>> deviceMap = {
        {"TYPE_A", DeviceTypeID::TYPE_A},
        {"TYPE_B", DeviceTypeID::TYPE_B}
    };

    // 如果输入为空，直接返回
    if (str.empty()) return DeviceTypeID::UNKNOWN;

    // 遍历查找表
    for (const auto& pair : deviceMap) {
        // 检查 pair.first (枚举名) 是否出现在 str (输入字符串) 中
        // std::string::npos 表示没找到
        if (str.find(pair.first) != std::string::npos) {
            return pair.second;
        }
    }

    return DeviceTypeID::UNKNOWN;
}
///////////////////////////////////////////////////////////////////////

TcpServer::TcpServer(uint16_t port)
    : fd_data_queue(30), port_(port) {
    tcp_epoll_fd_ = epoll_create1(0);
    tcp_stop_event_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    if (tcp_epoll_fd_ == -1 || tcp_stop_event_fd_ == -1) {
        std::cerr << "[TcpServer] 初始化 epoll/eventfd 失败: " << strerror(errno) << std::endl;
    }

    // 把 stop_event_fd 加入 epoll（用于优雅停止）
    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = tcp_stop_event_fd_;
    epoll_ctl(tcp_epoll_fd_, EPOLL_CTL_ADD, tcp_stop_event_fd_, &ev);
}

TcpServer::~TcpServer() {
    stop();
    if (tcp_epoll_fd_ != -1) close(tcp_epoll_fd_);
    if (tcp_stop_event_fd_ != -1) close(tcp_stop_event_fd_);
    if (listen_fd_ != -1) close(listen_fd_);
}
void TcpServer::registerDeviceType(const DeviceTypeID& type_id, DeviceCreator creator) {
    std::lock_guard<std::recursive_mutex> lock(conn_mutex_);
    device_factory_[type_id] = std::move(creator);
}
void TcpServer::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return; 

    // 创建监听 socket
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ == -1) {
        std::cerr << "[TcpServer] socket 创建失败: " << strerror(errno) << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setNonBlock(listen_fd_);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) == -1 ||
        listen(listen_fd_, SOMAXCONN) == -1) {
        std::cerr << "[TcpServer] bind/listen 失败: " << strerror(errno) << std::endl;
        close(listen_fd_);
        return;
    }

    // 把 listen_fd 加入 epoll
    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_fd_;
    epoll_ctl(tcp_epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev); 

    // 启动 TCP epoll 线程
    tcp_thread_ = std::thread(&TcpServer::tcpEventLoop, this);
    std::cout << "[TcpServer] 已启动，监听端口 " << port_ << std::endl;

    for(auto& _thread : thread_pool){
        _thread.start();
        _thread.set_loop_callback([this](SafeThread* self) ->bool{
            int fd;
            if(fd_data_queue.pop(fd)==false) {return true;}
            std::unique_lock<std::recursive_mutex> lock(conn_mutex_);
            auto it = connections_.find(fd);
            if (it == connections_.end()) return true;
            ConnectionInfo& info = it->second;

            std::vector<uint8_t> recv_buf = std::move(info.recv_buf);
            TcpDevice* device = info.device.get();
            lock.unlock();
            
            if(device){device->handleData(recv_buf);}

            return true;
        });
    }
}

void TcpServer::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) return;

    // 唤醒 epoll_wait
    uint64_t one = 1;
    write(tcp_stop_event_fd_, &one, sizeof(one));

    if (tcp_thread_.joinable()) {
        tcp_thread_.join();
    }

    // 清理所有连接
    std::lock_guard<std::recursive_mutex> lock(conn_mutex_);
    for (auto& p : connections_) {
        closeConnection(p.first);   // 内部会 erase，不用担心迭代器失效
    }
    connections_.clear();
}

void TcpServer::tcpEventLoop(){
    while (running_.load(std::memory_order_relaxed)){
        struct epoll_event events[16];
        int nfds = epoll_wait(tcp_epoll_fd_, events, 16, -1);

        if (nfds == -1) {
            if (errno == EINTR) continue;
            std::cerr << "[TcpServer] epoll_wait 错误: " << strerror(errno) << std::endl;
            break;
        }

        for (int i = 0; i < nfds; ++i){
            int fd = events[i].data.fd;

            if (fd == tcp_stop_event_fd_) {
                uint64_t dummy;
                read(tcp_stop_event_fd_, &dummy, sizeof(dummy));
                continue;
            }

            if (fd == listen_fd_) {
                handleNewConnection(); // 有新连接
                continue;
            }

            // 客户端事件
            if (events[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    handleClientError(fd, (events[i].events & EPOLLRDHUP) != 0);
                } else {
                    handleClientData(fd);
                }
            }
        }
    }
    std::cout << "线程结束\n";
}

void TcpServer::handleNewConnection() {
    while (true) {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(listen_fd_, (sockaddr*)&client_addr, &len);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            std::cerr << "[TcpServer] accept 失败: " << strerror(errno) << std::endl;
            return;
        }

        setNonBlock(client_fd);

        // 加入 epoll
        struct epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        ev.data.fd = client_fd;
        if (epoll_ctl(tcp_epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            close(client_fd);
            continue;
        } 

        {
            std::lock_guard<std::recursive_mutex> lock(conn_mutex_);
            connections_[client_fd] = ConnectionInfo{};
        }

        std::cout << "[TcpServer] 新连接 fd=" << client_fd
                  << " (" << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << ")" << std::endl;

        // 进入识别阶段：发送识别码
        startIdentificationTimer(client_fd);
    }
}
void TcpServer::handleClientData(int fd){
    std::lock_guard<std::recursive_mutex> lock(conn_mutex_);
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    ConnectionInfo& info = it->second;
    std::vector<uint8_t> buf(128);
    ssize_t n;
    
    TimerManager::getInstance().startTimer(info.timeout_timer);

    while ((n = read(fd, buf.data(), buf.size())) > 0){
        
        info.recv_buf.insert(info.recv_buf.end(), buf.begin(), buf.begin() + n);

        if (info.state == ConnState::IDENTIFYING){
            std::string str(info.recv_buf.begin(), info.recv_buf.end());
            DeviceTypeID id = stringToDeviceType(str);
            if(id!=DeviceTypeID::UNKNOWN){
                // 如果识别成功
                identifySuccess(fd, id);
            }else{
                std::clog << "无法找到对应的正确类型\n";
            }
            return;   // 识别成功后不再继续处理本次数据
        }
        
        // 已识别：把完整数据交给对应的 Device 解析
        if (info.state == ConnState::IDENTIFIED && info.device) {
            fd_data_queue.push(fd);
            // info.device->handleData(info.recv_buf);
            // info.recv_buf.clear();   // 简化处理：每次数据都完整交给上层（实际项目可按协议帧解析）
        }
        
    }

    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        handleClientError(fd, n == 0);
    }
    
}
void TcpServer::handleClientError(int fd, bool is_eof) {
    std::cout << "[TcpServer] 连接断开 fd=" << fd << (is_eof ? " (EOF)" : " (错误)") << std::endl;
    closeConnection(fd);
}
void TcpServer::sendRecognition(int fd) {
    std::cout << "识别码\n";
}

void TcpServer::sendHeartbeat(int fd) {
    std::cout << "心跳包\n";
}

void TcpServer::startIdentificationTimer(int fd) {
    // 1秒发一次
    auto tid = TimerManager::getInstance().createTimer(std::chrono::seconds(1),
                                                    std::chrono::seconds(1),
                                                    [this, fd](TimerManager::TimerId tid) {sendRecognition(fd);});
    TimerManager::getInstance().startTimer(tid);

    std::lock_guard<std::recursive_mutex> lock(conn_mutex_);
    auto it = connections_.find(fd);
    if (it != connections_.end()) {
        it->second.ident_timer = tid;
    }
}
void TcpServer::startHeartbeatTimer(int fd) {
    // 首次 5秒后触发，之后每 5 秒一次
    auto tid = TimerManager::getInstance().createTimer(std::chrono::seconds(5),
                                                std::chrono::seconds(5),
                                                [this, fd](TimerManager::TimerId tid) {
                                                    sendHeartbeat(fd);
                                                });
    TimerManager::getInstance().startTimer(tid);

    auto tid2 = TimerManager::getInstance().createTimer(std::chrono::seconds(6),
                                                std::chrono::seconds(0),
                                                [this, fd](TimerManager::TimerId tid) {
                                                    closeConnection(fd); // 超时无数据，认为断开
                                                });
    TimerManager::getInstance().startTimer(tid2);

    std::lock_guard<std::recursive_mutex> lock(conn_mutex_);
    auto it = connections_.find(fd);
    if (it != connections_.end()) {
        it->second.heartbeat_timer = tid;
        it->second.timeout_timer = tid2;
    }
}
void TcpServer::identifySuccess(int fd, DeviceTypeID deveice_type){
    std::lock_guard<std::recursive_mutex> lock(conn_mutex_);
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    ConnectionInfo& info = it->second;
 
    // 停止识别定时器
    if (info.ident_timer != 0) {
        TimerManager::getInstance().destroyTimer(info.ident_timer);
        info.ident_timer = 0;
    }

    info.state = ConnState::IDENTIFIED;

    // ====================== 【关键】实例化对应类对象 ======================
    auto factory_it = device_factory_.find(deveice_type);
    if (factory_it != device_factory_.end()){
        auto device = factory_it->second();   // 调用 lambda 创建对象
        if(device){
            info.device = std::move(device);
            info.device->fd = fd;
            info.device->onConnect();
        }else{
            std::cout << "生成工厂对象失败\n";
            closeConnection(fd);
            return;
        }
    }else{
        std::cout << "该设置类型没有注册过,无法生成工厂对象\n";
        closeConnection(fd);
        return;
    }

    std::cout << "[TcpServer] 识别成功 fd=" << fd << "，已实例化对应 Device, 开始心跳" << std::endl;

    // 启动心跳定时器
    startHeartbeatTimer(fd);
}
void TcpServer::closeConnection(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    ConnectionInfo& info = it->second;

    // 清理定时器
    if (info.ident_timer != 0) {
        TimerManager::getInstance().destroyTimer(info.ident_timer);
    }
    if (info.heartbeat_timer != 0) {
        TimerManager::getInstance().destroyTimer(info.heartbeat_timer);
    }
	if (info.timeout_timer != 0) {
        TimerManager::getInstance().destroyTimer(info.timeout_timer);
    }

    // 通知 Device 断开
    if (info.device) {
        info.device->onDisconnect();
    }

    // 移除 epoll
    epoll_ctl(tcp_epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);

    connections_.erase(it);
    std::cout << "[TcpServer] 连接已清理 fd=" << fd << std::endl;
}

void TcpServer::setNonBlock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
