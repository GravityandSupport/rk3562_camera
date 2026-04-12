#pragma once

#include <set>
#include <mutex>
#include <iostream>

#include "tcp_device.h"

class PC_UDP_ImageTrans : public TcpDevice{
public:
    PC_UDP_ImageTrans();
    virtual ~PC_UDP_ImageTrans();

    // 获取总数也是需要加锁的，防止读取到修改一半的值
    static int get_instance_count();

    // 返回拷贝以确保安全，或者由调用方手动管理生命周期
    static std::set<PC_UDP_ImageTrans*> get_all_instances();
private:
    static std::set<PC_UDP_ImageTrans*> instance_registry;
    static std::mutex registry_mutex; // 保护上述两个成员的静态锁
};

