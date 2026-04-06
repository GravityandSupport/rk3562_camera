#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    // 设置无边框 (去掉标题栏、控制按钮)
    w.setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // 设置全屏
    w.showFullScreen();

    w.show();
    return a.exec();
}
