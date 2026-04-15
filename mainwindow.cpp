#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QVBoxLayout>
#include <QLabel>

#include "tcp_device.h"
#include "pc_udp_imagetrans.h"
#include "udpsocket.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    TimerManager::getInstance().start();  // 定时器启动
    tcp_server.start(); // tcp服务器启动

    if(!UdpSocket::getInstance().create()){
        LOG_DEBUG("UDP", "udp打开失败");
    }
    if(!UdpSocket::getInstance().bind("0.0.0.0", 8888)){
        LOG_DEBUG("UDP", "udp绑定失败");
    }
    UdpSocket::getInstance().start();

    tcp_server.registerDeviceType(DeviceTypeID::UDP_IMAGE_TRANS, // 返回udp工厂对象句柄
    []() -> std::unique_ptr<TcpDevice>{
        return std::unique_ptr<TcpDevice>(new PC_UDP_ImageTrans());
    });

    capture = std::make_shared<V4L2_NV12_Capture>(1024, 592, 30, "/dev/video22");
    if(capture->register_device()!=0){
        std::cerr << "注册设备失败\n";
        return;
    }
    capture->start_stream();

    capture->add_video(&h264_encoder);

    h264_encoder.start_encoder(1024, 592, 30);

    // test
#if 1
    UdpSocket::getInstance().registerCallback("192.168.31.149", 777, [&](const char* data, size_t len,
                                              const std::string& sender_ip,
                                                  uint16_t sender_port){
        std::string str(data, data+len);
        LOG_DEBUG("UDP RECV", sender_ip, sender_port, str);
    });
#endif
}

MainWindow::~MainWindow()
{
    delete ui;
}

