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

    if(capture.create(1920, 1072, 6, "/dev/video22")!=0){
        std::cerr << "注册设备失败\n";
        return;
    }
    capture.start_stream();
    if(capture_33.create(1920, 1072, 6, "/dev/video33")!=0){
        std::cerr << "注册设备失败\n";
        return;
    }
    capture_33.start_stream();

    uvc_monitor.start();
    mjpeg_decoder.create(1920, 1072, 2);
//    usb_camera.add_video(&mjpeg_decoder);
//    usb_camera.register_device(1920, 1072, 6, "dev/video40");

#if 0
    mjpeg_decoder.add_video(&video_merge);
    video_merge.setBigNode(&mjpeg_decoder, 0, 0, 640, 480);
#else
    capture_33.add_video(&video_merge);
    video_merge.setBigNode(&capture_33, 0, 0, 640, 480);
#endif
    capture.add_video(&video_merge);
    video_merge.setSmallNode(&capture, 0, 0, 192, 144);

    video_merge.add_video(&h264_encoder);
    video_merge.create(640,  480);

    h264_encoder.start_encoder(640, 480, 30);

    dmaBuf_render = std::make_shared<DmaBufRenderer>(this);
    dmaBuf_render->resize(1024, 600);

    image_display.create(dmaBuf_render.get());
    video_merge.add_video(&image_display);

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

