#pragma once

#include <vector>
#include <cstdint>
#include <iostream>
#include <iomanip> // 必须包含，用于格式化输出
#include <string>

#include <netinet/in.h>

class TcpDevice {
public:
    TcpDevice() = default;
    virtual ~TcpDevice() = default;

    int fd=-1;
    in_port_t       sin_port;       /* Port number */
    struct in_addr  sin_addr;       /* IPv4 address */

    // TCP 线程收到完整数据后调用（已去掉 TCP 粘包/分包问题，用户只需关注业务）
    virtual void handleData(const std::vector<uint8_t>& data) {
        if (data.empty()) {
            std::cout << "Data is empty." << std::endl;
            return;
        }

        std::string str(data.begin(), data.end());
        std::cout << "fd=" << fd << ", 接收的数据=" << str << std::endl;

        // std::cout << "Received data (Hex): ";
        // for (auto byte : data) {
        //     // std::hex: 十六进制显示
        //     // std::setw(2): 保证每个字节占两位
        //     // std::setfill('0'): 不足两位补0
        //     // +byte: 将 uint8_t 提升为 int，否则 cout 会尝试将其打印为字符
        //     std::cout << std::hex << std::setw(2) << std::setfill('0') 
        //             << static_cast<int>(byte) << " ";
        // }
        // std::cout << std::dec << std::endl; // 打印完记得恢复成十进制
    }

    virtual void onDisconnect() {}
    virtual void onConnect() {}
};
