#include "tcp_device.h"


void TcpDevice::handleParseData(const std::vector<uint8_t>& data){
    for(size_t i=0; i<data.size(); ++i){
        switch (rec_status) {
        case RecStatus::none:{
            if(data[i]==0x5a){
                rec_status = RecStatus::x5a;
            }
            break;
        }
        case RecStatus::x5a:{
            if(data[i]==0xa5){
                rec_status = RecStatus::xa5;
            }else{
                rec_status = RecStatus::none;
            }
            break;
        }
        case RecStatus::xa5:{
            rec_addr = data[i];
            rec_status = RecStatus::addr_h;
            break;
        }
        case RecStatus::addr_h:{
            rec_addr = rec_addr*256 + data[i];
            rec_status = RecStatus::addr_l;
            break;
        }
        case RecStatus::addr_l:{
            data_len = data[i];
            rec_status = RecStatus::len;
            rec_data.clear();
            break;
        }
        case RecStatus::len:{
            if(rec_data.size()<data_len){
                rec_data.emplace_back(data[i]);
            }
            if(rec_data.size()>=data_len){
                handleData(rec_addr, rec_data);
                rec_status = RecStatus::none;
            }
            break;
        }
        default:break;
        }
    }
}

void TcpDevice::sendRecognition(int fd){
    TcpDevice device;
    device.fd = fd;
    device.sendData(0xa000, {});
}
void TcpDevice::sendHeartbeat(){
    sendData(0xa0a0, {});
}

void TcpDevice::sendData(uint16_t addr, const std::vector<uint8_t>& data){
    if (fd < 0) return; // 基础检查
    std::vector<uint8_t> send_buf;
    send_buf.reserve(5 + data.size());

    send_buf.push_back(0x5a);
    send_buf.push_back(0xa5);
    send_buf.push_back(static_cast<uint8_t>(addr >> 8));   // 高8位
    send_buf.push_back(static_cast<uint8_t>(addr & 0xFF)); // 低8位
    send_buf.push_back(static_cast<uint8_t>(data.size()));
    send_buf.insert(send_buf.end(), data.begin(), data.end());

    const uint8_t* ptr = send_buf.data();
    size_t total_to_send = send_buf.size();
    size_t total_sent = 0;

    while (total_sent < total_to_send) {
        ssize_t n = ::send(fd, ptr + total_sent, total_to_send - total_sent, MSG_NOSIGNAL);

        if (n <= 0) {
            if (n < 0 && (errno == EINTR || errno == EAGAIN)) {
                continue; // 信号中断或缓冲区满，重试
            }
            // 真正发生了错误（如断开连接）
            // 这里可以记录日志或处理断开逻辑
            return;
        }
        total_sent += n;
    }
}
