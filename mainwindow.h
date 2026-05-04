#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

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

    // === 公共接口 ===

    /// 显示/隐藏录像时长（左下角 MM:SS）
    void showRecordingTime(bool show);

    /// 更新录像时长显示（传入已录像的秒数）
    void updateRecordingTime(int seconds);

    /// 重置录像计时为0并隐藏
    void resetRecordingTime();

    /// 显示居中的通知（例如拍照完成/录像完成）
    /// @param text      显示的文字内容（如文件保存路径）
    /// @param durationMs 自动隐藏的毫秒数，<=0 表示不会自动隐藏，需手动调用 hideNotification()
    void showNotification(const QString &text, int durationMs = 3000);

    /// 手动隐藏通知
    void hideNotification();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUI();

private slots:
    void onClockTick();
    void onRecordingTick();
    void onNotificationTimeout();

private:
    Ui::MainWindow *ui;

    // === UI 控件 ===
    QLabel      *m_recordingTimeLabel;   // 左下角 - 录像时长
    QLabel      *m_dateTimeLabel;        // 右上角 - 日期时间
    QLabel      *m_notificationLabel;    // 居中 - 通知提示
    QPushButton *m_btnAction1;           // 右侧按钮1
    QPushButton *m_btnAction2;           // 右侧按钮2

    // === 定时器 ===
    QTimer      *m_clockTimer;           // 时钟刷新
    QTimer      *m_recordingTimer;       // 录像计时
    QTimer      *m_notificationTimer;    // 通知自动隐藏

    // === 状态 ===
    int m_recordingSeconds = 0;
};
#endif // MAINWINDOW_H
