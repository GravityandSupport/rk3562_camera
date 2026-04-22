#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "interface.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    std::shared_ptr<DmaBufRenderer> dmaBuf_render;
private:
    Ui::MainWindow *ui;


};
#endif // MAINWINDOW_H
