#include "pc_udp_imagetrans.h"
#include "outLog.h"
#include "interface.h"
#include "udpsocket.h"

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
    sendData(0x4000, {});
//    udp_port = sin_port;
    h264_encoder.add_video(this);
}

void PC_UDP_ImageTrans::onDisconnect(){
    h264_encoder.remove_video(this);
}


struct NALUInfo {
    size_t offset;        // 起始码首字节在 buf 中的偏移
    size_t sc_len;        // 起始码长度 (3 或 4)
};

static std::vector<NALUInfo> find_nalu_offsets(const uint8_t* buf, size_t len)
{
    std::vector<NALUInfo> result;
    if (len < 4) return result;

    for (size_t i = 0; i < len - 3; ) {
        if (buf[i] == 0x00 && buf[i+1] == 0x00) {
            if (i + 3 < len && buf[i+2] == 0x00 && buf[i+3] == 0x01) {
                result.push_back({i, 4});
                i += 4;
                continue;
            }
            if (buf[i+2] == 0x01) {
                result.push_back({i, 3});
                i += 3;
                continue;
            }
        }
        ++i;
    }
    return result;
}


void PC_UDP_ImageTrans::process_frames(VideoFramePtr frame){
    auto offsets = find_nalu_offsets(frame->data->data(), frame->data->size());
    for (size_t i = 0; i < offsets.size(); ++i){
        printf("NAL Header = 0x%02x\n", frame->data->at(offsets[i].offset+offsets[i].sc_len));
    }


    if(is_get_udp_port==false) {return;}
//    LOG_DEBUG("udp image trans", frame->width, frame->height, frame->data->size());
    uint8_t arr[] = {0xaa, 0xbb, 0xcc, 0xdd};
//    LOG_DEBUG("udp image trans", inet_ntoa(udp_addr), ntohs(udp_port));
    UdpSocket::getInstance().sendTo(udp_addr, udp_port, arr, sizeof(arr));
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
//        printf("port=0x%04x\n", port);
        LOG_DEBUG("UDP IMAGE TRANS", port);
        udp_port = htons(port);
        is_get_udp_port = true;
        break;
     }
     default:break;
     }
}
