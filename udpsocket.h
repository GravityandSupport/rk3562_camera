#pragma once

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <atomic>
#include <thread>


class UdpSocket
{
public:
    static UdpSocket& getInstance() {static UdpSocket instance; return instance;}

    // 接收回调类型：data, len, sender_ip, sender_port
    using RecvCallback = std::function<void(const char* data, size_t len,
                                    const std::string& sender_ip,
                                    uint16_t sender_port)>;


    // ==================== 基础操作 ====================
    bool create();
    bool bind(const std::string& ip, uint16_t port);
    bool setNonBlocking();

    // ==================== 发送 ====================
        ssize_t sendTo(const std::string& ip, uint16_t port,
                       const void* data, size_t len);

    // ==================== 回调注册（精确到 IP:Port） ====================
    /**
     * 注册回调 - 精确匹配 IP:Port
     * @param remote_ip   远端 IP
     * @param remote_port 远端端口
     * @param callback    回调函数
     */
    void registerCallback(const std::string& remote_ip, uint16_t remote_port,
                          RecvCallback callback);

    // 移除指定 IP:Port 的回调
    void removeCallback(const std::string& remote_ip, uint16_t remote_port);

    // 移除某个 IP 的所有端口回调
    void removeAllCallbacksForIp(const std::string& remote_ip);

    bool start();
    void stop();
    int fd() const;

private:
    UdpSocket();
    virtual ~UdpSocket();
    // 内部实现类（Pimpl 惯用法，隐藏实现细节）
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};


