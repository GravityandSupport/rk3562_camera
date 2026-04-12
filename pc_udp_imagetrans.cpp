#include "pc_udp_imagetrans.h"

std::set<PC_UDP_ImageTrans*> PC_UDP_ImageTrans::instance_registry;
std::mutex PC_UDP_ImageTrans::registry_mutex;

PC_UDP_ImageTrans::PC_UDP_ImageTrans() {
    // 自动加锁，作用域结束自动解锁
    std::lock_guard<std::mutex> lock(registry_mutex);

    instance_registry.insert(this);

    // 调试打印最好也放在锁内，防止多线程打印混乱
//    std::cout << "[BaseDevice] Created: " << this << " | Total: " << instance_count << std::endl;
}

PC_UDP_ImageTrans::~PC_UDP_ImageTrans() {
    std::lock_guard<std::mutex> lock(registry_mutex);

    instance_registry.erase(this);

//    std::cout << "[BaseDevice] Destroyed: " << this << " | Total: " << instance_count << std::endl;
}

int PC_UDP_ImageTrans::get_instance_count() {
    std::lock_guard<std::mutex> lock(registry_mutex);
    return instance_registry.size();
}

std::set<PC_UDP_ImageTrans*> PC_UDP_ImageTrans::get_all_instances() {
    std::lock_guard<std::mutex> lock(registry_mutex);
    return instance_registry;
}



