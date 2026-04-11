#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QVBoxLayout>
#include <QLabel>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    TimerManager::getInstance().start();  // 定时器启动
    tcp_server.start(); // tcp服务器启动

    capture = std::make_shared<V4L2_NV12_Capture>(1024, 592, 30, "/dev/video22");
    if(capture->register_device()!=0){
        std::cerr << "注册设备失败\n";
        return;
    }
    capture->start_stream();

    capture->add_video(&h264_encoder);

    h264_encoder.start_encoder(1024, 592, 30);
}

MainWindow::~MainWindow()
{
    delete ui;
}

