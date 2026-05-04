#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QFont>
#include <QResizeEvent>

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
    video_merge.setBigNode(&capture_33, 0, 0, 1024, 592);
#endif
    capture.add_video(&video_merge);
    video_merge.setSmallNode(&capture, 0, 0, 320, 224);

    video_merge.add_video(&h264_encoder);
    video_merge.create(1024, 592);

    mp4_muxer.create(&h264_encoder);
    h264_nalu_save.create(&h264_encoder);
    h264_encoder.start_encoder(1024, 592, 30);
    h264_encoder.add_video(&h264_nalu_save);
    h264_encoder.add_video(&mp4_muxer);

#if 1
//    mp4_muxer.save("xxx");
#endif

//    h264_decoder.create(1024, 592, 2);
//    h264_encoder.add_video(&h264_decoder);

    dmaBuf_render = std::make_shared<DmaBufRenderer>(this);
    dmaBuf_render->resize(1024, 600);

    image_display.create(dmaBuf_render.get());
    video_merge.add_video(&image_display);

    mjpeg_encoder.create(1024, 592, 10);
    video_merge.add_video(&mjpeg_encoder);
    mjpeg_encoder.add_video(&photo_save);
    photo_save.create(&mjpeg_encoder);

    tcp_client.connect("192.168.31.149", 7777);

    // ========== UI 控件初始化 ==========
    setupUI();

    // ========== UI 控件初始化 ==========

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

    if(event->key() == Qt::Key_VolumeUp){
        if(photo_album->isHidden()){
            photo_album->loadPathFile("/mnt/nfs_dir");
            photo_album->show();
        }else{
            photo_album->hide();
        }
    }else if (event->key() == Qt::Key_VolumeDown) {
#if 0
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm bt = *std::localtime(&in_time_t);

        std::stringstream ss;
        ss << "/mnt/nfs_dir/" << std::put_time(&bt, "%Y-%m-%d-%S%M") << ".h264";
        h264_nalu_save.save(ss.str());
#else
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm bt = *std::localtime(&in_time_t);

        std::stringstream ss;
        ss << "/mnt/nfs_dir/" << std::put_time(&bt, "%Y-%m-%d-%S%M") << ".jpg";
        if(photo_save.save(ss.str())){
            showNotification(QString::fromStdString(std::string("照片已保存到")+ss.str()));
        }
#endif
    }else if (event->key() == Qt::Key_MenuKB){
        static std::string path="none";
        if(!mp4_muxer.isRunning()){
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm bt = *std::localtime(&in_time_t);

            std::stringstream ss;
            ss << "/mnt/nfs_dir/" << std::put_time(&bt, "%Y-%m-%d-%S%M") << ".mp4";
            path = ss.str();
            LOG_DEBUG("MP4", ss.str());
            if(mp4_muxer.save(ss.str())){
                showRecordingTime(true);
            }
        }else{
            mp4_muxer.finish();
            showRecordingTime(false);
            showNotification(QString::fromStdString(std::string("录像已保存到")+path));
        }
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
            return;  // 忽略长按重复

    qDebug() << "按键释放:" << event->key();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    // 更新录像时长标签位置（左下角）
    if (m_recordingTimeLabel)
        m_recordingTimeLabel->move(14, height() - 34);

    // 更新时间标签位置（右上角）
    if (m_dateTimeLabel)
        m_dateTimeLabel->setGeometry(0, 8, width() - 14, 28);

    // 更新通知标签位置（居中）
    if (m_notificationLabel && m_notificationLabel->isVisible()) {
        m_notificationLabel->move((width() - m_notificationLabel->width()) / 2,
                                  (height() - m_notificationLabel->height()) / 2);
    }

    // 更新右侧按钮位置（垂直居中）
    if (m_btnAction1 && m_btnAction2) {
        const int btnW = 80;
        const int btnH = 44;
        const int btnGap = 16;
        int btnX = width() - btnW - 14;
        int centerY = height() / 2;
        m_btnAction1->move(btnX, centerY - btnH - btnGap / 2);
        m_btnAction2->move(btnX, centerY + btnGap / 2);
    }
}

// ====================================================================
//  UI 初始化
// ====================================================================
static QLabel* createLabel(QWidget *parent, const QString &text,
                           const QColor &color, int fontSize,
                           Qt::Alignment align)
{
    QLabel *label = new QLabel(text, parent);
    label->setStyleSheet(QString("color: %1; background: transparent;")
                         .arg(color.name()));
    QFont f = label->font();
    f.setPointSize(fontSize);
    f.setBold(true);
    label->setFont(f);
    label->setAlignment(align);
    return label;
}

void MainWindow::setupUI()
{
    // ======== 1. 右上角 — 日期时间 ========
    m_dateTimeLabel = createLabel(this, "----",
                                  QColor("#00FF00"), 24,
                                  Qt::AlignRight | Qt::AlignVCenter);
    m_dateTimeLabel->setGeometry(0, 8, 1010, 28);   // 右上留边距

    // ======== 2. 左下角 — 录像时长 ========
    m_recordingTimeLabel = createLabel(this, "00:00",
                                       QColor("#FF0000"), 20,
                                       Qt::AlignLeft | Qt::AlignVCenter);
    m_recordingTimeLabel->setGeometry(14, height() - 34, 240, 28);
    m_recordingTimeLabel->hide();   // 默认隐藏

    // ======== 3. 居中 — 通知提示 ========
    m_notificationLabel = new QLabel(this);
    m_notificationLabel->setStyleSheet(
        "QLabel {"
        "  color: #FFFFFF;"
        "  background: rgba(0, 0, 0, 80);"
        "  border: 2px solid #00FF00;"
        "  border-radius: 8px;"
        "  padding: 16px 32px;"
        "}");
    QFont nf = m_notificationLabel->font();
    nf.setPointSize(18);
    nf.setBold(true);
    m_notificationLabel->setFont(nf);
    m_notificationLabel->setAlignment(Qt::AlignCenter);
    m_notificationLabel->setWordWrap(true);
    m_notificationLabel->setFixedWidth(480);
    m_notificationLabel->adjustSize();
    // 居中定位（稍后在 resizeEvent 中动态更新）
    m_notificationLabel->move((width() - m_notificationLabel->width()) / 2,
                              (height() - m_notificationLabel->height()) / 2);
    m_notificationLabel->hide();

    // ======== 4. 右侧 — 两个按钮（垂直居中） ========
    const int btnW = 80;
    const int btnH = 44;
    const int btnGap = 16;
    int btnX = width() - btnW - 14;   // 右边缘留边距

    m_btnAction1 = new QPushButton("画面数量", this);
    m_btnAction1->setFixedSize(btnW, btnH);
    m_btnAction1->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #F7DC77, stop:1 #F7DC77);"
        "  color: white;"
        "  border: none;"
        "  border-radius: 8px;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #F59F42, stop:1 #F59F42);"
        "}");

    m_btnAction2 = new QPushButton("画面切换", this);
    m_btnAction2->setFixedSize(btnW, btnH);
    m_btnAction2->setStyleSheet(m_btnAction1->styleSheet());

    connect(m_btnAction1, &QPushButton::clicked, this, [&](){
        int channel_count = video_merge.getChannelCount();
        if(channel_count==1){
            video_merge.setChannelCount(2);
        }else if(channel_count==2){
            video_merge.setChannelCount(1);
        }
    });
    connect(m_btnAction2, &QPushButton::clicked, this, [&](){
        video_merge.setSwapNode();
    });

    // 垂直居中定位
    int centerY = height() / 2;
    m_btnAction1->move(btnX, centerY - btnH - btnGap / 2);
    m_btnAction2->move(btnX, centerY + btnGap / 2);

    // ======== 5. 定时器 ========
    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &MainWindow::onClockTick);
    m_clockTimer->start(1000);   // 每秒更新
    onClockTick();               // 立即显示一次

    m_recordingTimer = new QTimer(this);
    connect(m_recordingTimer, &QTimer::timeout, this, &MainWindow::onRecordingTick);

    m_notificationTimer = new QTimer(this);
    m_notificationTimer->setSingleShot(true);
    connect(m_notificationTimer, &QTimer::timeout,
            this, &MainWindow::onNotificationTimeout);
}

// ====================================================================
//  录像时长显示
// ====================================================================
void MainWindow::showRecordingTime(bool show)
{
    m_recordingTimeLabel->setVisible(show);
    if (show) {
        m_recordingSeconds = 0;
        m_recordingTimeLabel->setText("00:00");
        m_recordingTimer->start(1000);
    } else {
        m_recordingTimer->stop();
    }
}

void MainWindow::updateRecordingTime(int seconds)
{
    m_recordingSeconds = seconds;
    int mm = seconds / 60;
    int ss = seconds % 60;
    m_recordingTimeLabel->setText(QString("录像中%1:%2")
        .arg(mm, 2, 10, QChar('0'))
        .arg(ss, 2, 10, QChar('0')));
}

void MainWindow::resetRecordingTime()
{
    m_recordingTimer->stop();
    m_recordingSeconds = 0;
    m_recordingTimeLabel->setText("00:00");
    m_recordingTimeLabel->hide();
}

void MainWindow::onRecordingTick()
{
    m_recordingSeconds++;
    updateRecordingTime(m_recordingSeconds);
}

// ====================================================================
//  通知提示
// ====================================================================
void MainWindow::showNotification(const QString &text, int durationMs)
{
    m_notificationLabel->setText(text);
    m_notificationLabel->adjustSize();
    // 确保宽度不超过设定值
    if (m_notificationLabel->width() > 480)
        m_notificationLabel->setFixedWidth(480);
    m_notificationLabel->adjustSize();

    // 居中
    m_notificationLabel->move((width() - m_notificationLabel->width()) / 2,
                              (height() - m_notificationLabel->height()) / 2);
    m_notificationLabel->show();
    m_notificationLabel->raise();

    // 自动隐藏
    m_notificationTimer->stop();
    if (durationMs > 0)
        m_notificationTimer->start(durationMs);
}

void MainWindow::hideNotification()
{
    m_notificationTimer->stop();
    m_notificationLabel->hide();
}

void MainWindow::onNotificationTimeout()
{
    m_notificationLabel->hide();
}

// ====================================================================
//  时钟
// ====================================================================
void MainWindow::onClockTick()
{
    QDateTime now = QDateTime::currentDateTime();
    QString dt = now.toString("yyyy-MM-dd HH:mm:ss");
    m_dateTimeLabel->setText(dt);
}

