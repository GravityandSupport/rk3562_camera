#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QVBoxLayout>
#include <QLabel>

#include "tcp_device.h"
#include "pc_udp_imagetrans.h"
#include "udpsocket.h"

#include "rgacontrol.h"

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

    capture = std::make_shared<V4L2_NV12_Capture>(1920, 1072, 6, "/dev/video22");
    if(capture->register_device()!=0){
        std::cerr << "注册设备失败\n";
        return;
    }
    capture->start_stream();
    capture_33 = std::make_shared<V4L2_NV12_Capture>(1920, 1072, 6, "/dev/video33");
    if(capture_33->register_device()!=0){
        std::cerr << "注册设备失败\n";
        return;
    }
    capture_33->start_stream();


    capture->add_video(&video_merge);
    capture_33->add_video(&video_merge);
    video_merge.big_source.video = capture_33.get();
    video_merge.big_source.x = 0;
    video_merge.big_source.y = 0;
    video_merge.big_source.width_ = 640;
    video_merge.big_source.height_ = 480;
    video_merge.small_source.video = capture.get();
    video_merge.small_source.x = 0;
    video_merge.small_source.y = 0;
    video_merge.small_source.width_ = 192;
    video_merge.small_source.height_ = 144;
    video_merge.add_video(&h264_encoder);
    video_merge.create(640,  480);

    h264_encoder.start_encoder(640, 480, 30);


    tcp_client.connect("192.168.31.149", 7777);
    // test
#if 1
    UdpSocket::getInstance().registerCallback("192.168.31.149", 777, [&](const char* data, size_t len,
                                              const std::string& sender_ip,
                                                  uint16_t sender_port,
                                              EpollEvent::Message message){
        if(message==EpollEvent::Message::Data){
            std::string str(data, data+len);
            LOG_DEBUG("UDP RECV", sender_ip, sender_port, str);
        }
    });
#endif

//    RgaControl::test();
}

MainWindow::~MainWindow()
{
    delete ui;
}

