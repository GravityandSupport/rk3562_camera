#pragma once

#include <set>
#include <mutex>
#include <iostream>

#include "tcp_device.h"
#include "videobase.h"

class PC_UDP_ImageTrans : public TcpDevice , public VideoBase{
public:


    PC_UDP_ImageTrans();
    virtual ~PC_UDP_ImageTrans();

    // 获取总数也是需要加锁的，防止读取到修改一半的值
    static int get_instance_count();

    // 返回拷贝以确保安全，或者由调用方手动管理生命周期
    static std::set<PC_UDP_ImageTrans*> get_all_instances();
protected:
    virtual void onConnect() override;
    virtual void onDisconnect()  override;

    virtual void handleData(uint16_t addr, const std::vector<uint8_t>& data ) override;

    virtual void process_frames(VideoFramePtr frame) override;
private:
    static std::set<PC_UDP_ImageTrans*> instance_registry;
    static std::mutex registry_mutex; // 保护上述两个成员的静态锁

    in_port_t       udp_port;       /* Port number */
    struct in_addr  udp_addr;       /* IPv4 address */

    void procesData();


};

