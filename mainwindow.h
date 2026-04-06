#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "v4l2_nv12_capture.h"


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
};
#endif // MAINWINDOW_H
