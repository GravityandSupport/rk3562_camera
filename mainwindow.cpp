#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QVBoxLayout>
#include <QLabel>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    capture = std::make_shared<V4L2_NV12_Capture>(1024, 592, 30, "/dev/video22");
    if(capture->register_device()!=0){
        std::cerr << "注册设备失败\n";
        return;
    }
    capture->start_stream();
    capture->run("capture");
}

MainWindow::~MainWindow()
{
    delete ui;
}

