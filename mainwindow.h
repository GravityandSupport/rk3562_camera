#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "v4l2_nv12_capture.h"
#include "h264_encoder.h"
#include "timermanager.h"
#include "tcp_server.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    std::shared_ptr<V4L2_NV12_Capture> capture;
    H264_Encoder h264_encoder;

    TcpServer tcp_server;
};
#endif // MAINWINDOW_H
