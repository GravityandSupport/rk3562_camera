#pragma once

#include "timermanager.h"
#include "tcp_device.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <cstdint>
#include <string>
#include <iostream>
#include <cstring>
#include <array>

#include "ThreadSafeBoundedQueue.h"
#include "safe_thread.h"

enum class DeviceTypeID : int{
    UDP_IMAGE_TRANS, TYPE_B,
    UNKNOWN
};
namespace std {
    template <>
    struct hash<DeviceTypeID> {
        size_t operator()(DeviceTypeID t) const {
            // 将枚举强转为底层整数类型 int，然后调用标准库的 int 哈希
            return std::hash<int>()(static_cast<int>(t));
        }
    };
}
DeviceTypeID stringToDeviceType(const std::string& str);

class TcpServer {
public:
    explicit TcpServer(uint16_t port = 8080);
    ~TcpServer();

    // 启动 TCP epoll 线程（推荐与 TimerManager 配合使用）
    void start();

    // 停止 TCP 线程
    void stop();

    // 工厂注册接口
    using DeviceCreator = std::function<std::unique_ptr<TcpDevice>()>;
    void registerDeviceType(const DeviceTypeID& type_id, DeviceCreator creator);
private:
    std::array<SafeThread, 2> thread_pool; // 微型线程池
    ThreadSafeBoundedQueue<int> fd_data_queue; // 哪些文件描述符有数据

    std::unordered_map<DeviceTypeID, DeviceCreator> device_factory_; // 工厂映射表

    // 连接状态
    enum class ConnState {
        IDENTIFYING,   // 识别阶段（发送识别码等待回复 或 等待从机发送识别码）
        IDENTIFIED     // 识别成功，已实例化对应 Device
    };


    struct ConnectionInfo {
        ConnState state = ConnState::IDENTIFYING;
        TimerManager::TimerId ident_timer = 0;      // 识别超时定时器（单次）
        TimerManager::TimerId heartbeat_timer = 0;  // 心跳发送定时器（5秒周期）
        TimerManager::TimerId timeout_timer = 0;  // 超时检测，6秒内收不到心跳包认为断开
        std::vector<uint8_t> recv_buf;              // 接收缓冲区（处理 TCP 粘包）
        std::unique_ptr<TcpDevice> device = nullptr;   // 识别成功后实例化的对应类对象

        in_port_t       sin_port;       /* Port number */
        struct in_addr  sin_addr;       /* IPv4 address */

    };


    int listen_fd_ = -1;
    int tcp_epoll_fd_ = -1;
    int tcp_stop_event_fd_ = -1;
    uint16_t port_;

    std::atomic<bool> running_{false};
    std::thread tcp_thread_;

    std::unordered_map<int, ConnectionInfo> connections_;   // fd -> 连接信息
    std::recursive_mutex conn_mutex_;                       // 保护 connections_

    // ==================== 内部函数 ====================
    void tcpEventLoop();                                    // TCP epoll 主循环

    void handleNewConnection();                             // 新连接
    void handleClientData(int fd);                          // 处理客户端数据
    void handleClientError(int fd, bool is_eof);            // 错误/断开

    void sendRecognition(int fd);                           // 发送识别码
    void sendHeartbeat(int fd);                             // 发送心跳包

    void startIdentificationTimer(int fd);                  // 启动识别超时定时器（1秒发一次）
    void startHeartbeatTimer(int fd);                       // 启动5秒心跳定时器

    void identifySuccess(int fd, DeviceTypeID deveice_type);
    void closeConnection(int fd);                           // 统一关闭连接（清理定时器 + Device + epoll）

    static void setNonBlock(int fd);
private:


};
