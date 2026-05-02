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

    h264_nalu_save.create();
    h264_encoder.start_encoder(640, 480, 30);
    h264_encoder.add_video(&h264_nalu_save);

    h264_decoder.create(640, 480, 2);
//    h264_encoder.add_video(&h264_decoder);

    dmaBuf_render = std::make_shared<DmaBufRenderer>(this);
    dmaBuf_render->resize(1024, 600);

    image_display.create(dmaBuf_render.get());
    video_merge.add_video(&image_display);

    mjpeg_encoder.create(640, 480, 10);
    video_merge.add_video(&mjpeg_encoder);

    tcp_client.connect("192.168.31.149", 7777);

    photo_album = std::make_shared<PhotoAlbum>("/mnt/nfs_dir", this);
    photo_album->setGeometry(0, 0, 1024, 600);
    photo_album->hide();
    photo_album->create();
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


void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
            return;  // 忽略长按重复

    qDebug() << "按键按下:" << event->key();

    if (event->key() == Qt::Key_VolumeDown) {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm bt = *std::localtime(&in_time_t);

        std::vector<std::string> filename;
        for(int i=0; i<5; i++){
            std::stringstream ss;
            ss << "/mnt/nfs_dir/" << std::put_time(&bt, "%Y-%m-%d-%S%M") << "-" << i  << ".h264";
            filename.push_back(ss.str());
        }
        JsonWrapper js;
        js.import("filename", filename);
//        LOG_DEBUG("debug", js.dump());
        EventBus::instance().publish("/video/h264/nalu_save", js.dump());
        qDebug() << "按下了 Key_VolumeDown";
    }else if(event->key() == Qt::Key_VolumeUp){
        if(photo_album->isHidden()){
            photo_album->loadPathFile("/mnt/nfs_dir");
            photo_album->show();
        }else{
            photo_album->hide();
        }
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
            return;  // 忽略长按重复

    qDebug() << "按键释放:" << event->key();
}

