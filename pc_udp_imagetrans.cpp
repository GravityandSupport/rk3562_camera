#include "pc_udp_imagetrans.h"
#include "outLog.h"
#include "interface.h"

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

void PC_UDP_ImageTrans::onConnect(){
    udp_addr = sin_addr;
//    udp_port = sin_port;
    h264_encoder.add_video(this);
}

void PC_UDP_ImageTrans::onDisconnect(){
    h264_encoder.remove_video(this);
}

void PC_UDP_ImageTrans::process_frames(VideoFramePtr frame){
    LOG_DEBUG("udp image trans", frame->width, frame->height, frame->data->size());
}

void PC_UDP_ImageTrans::handleData(uint16_t addr, const std::vector<uint8_t>& data ){
     std::cout << "Received data (Hex): ";
     std::cout << std::hex << std::setw(2) << std::setfill('0') << std::endl; std::cout << addr << ":";
     for (auto byte : data) {
         // std::hex: 十六进制显示
         // std::setw(2): 保证每个字节占两位
         // std::setfill('0'): 不足两位补0
         // +byte: 将 uint8_t 提升为 int，否则 cout 会尝试将其打印为字符
         std::cout << static_cast<int>(byte) << " ";
     }
     std::cout << std::dec << std::endl; // 打印完记得恢复成十进制

     switch(addr){
     case 0x4000:{ // port
        uint16_t port;
        if(data.size()<2) {return;}
        port = data[0]*256;
        port += data[1];
        printf("port=0x%04x\n", port);
        udp_port = htons(port);
        break;
     }
     default:break;
     }
}
