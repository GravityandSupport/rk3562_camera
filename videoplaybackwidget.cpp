#include "videoplaybackwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "JsonWrapper.h"
#include <experimental/filesystem>
#include "outLog.h"
#include "EventBus.h"

namespace fs = std::experimental::filesystem;

VideoPlaybackWidget::VideoPlaybackWidget(QWidget *parent) : QWidget(parent)
{
    dmaBuf_render = std::make_shared<DmaBufRenderer>(this);
    dmaBuf_render->resize(1024, 600);
    dmaBuf_render->show();

    initUi();
    initLayout();
}
void VideoPlaybackWidget::initUi(){
    filename_lable = new QLabel(this);
    filename_lable->setStyleSheet(
        "background-color: rgba(0, 0, 0, 100); " // 黑色半透明
        "border-radius: 15px; "                 // 圆角半径
        "color: white; "                        // 既然背景黑，文字建议设为白色
        "padding-left: 10px;"                   // 防止文字紧贴圆角边框
    );
    filename_lable->setGeometry(0, 0, 600, 70);
    filename_lable->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QFont f = filename_lable->font();
    f.setPointSize(18);
    f.setBold(true);
    filename_lable->setFont(f);

    m_btnLeft = std::make_shared<QPushButton>(this);
    m_btnRight = std::make_shared<QPushButton>(this);
    m_btnClose = std::make_shared<QPushButton>(this);

    m_btnLeft->setFixedSize(50, 50);
    m_btnRight->setFixedSize(50, 50);
    m_btnClose->setFixedSize(50, 50);

    m_btnLeft->setStyleSheet(
        "QPushButton {"
        "   border-image: url(/mnt/nfs_dir/res/ui/left.png);"
        "   background: transparent;"
        "   border: none;"
        "}"
        "QPushButton:pressed {"
        "   border-image: url(/mnt/nfs_dir/res/ui/left_p.png);"
        "}"
    );
    m_btnRight->setStyleSheet(
        "QPushButton {"
        "   border-image: url(/mnt/nfs_dir/res/ui/right.png);"
        "   background: transparent;"
        "   border: none;"
        "}"
        "QPushButton:pressed {"
        "   border-image: url(/mnt/nfs_dir/res/ui/right_p.png);"
        "}"
    );
    m_btnClose->setStyleSheet(
        "QPushButton {"
        "   border-image: url(/mnt/nfs_dir/res/ui/close.png);"
        "   background: transparent;"
        "   border: none;"
        "}"
        "QPushButton:pressed {"
        "   border-image: url(/mnt/nfs_dir/res/ui/close_p.png);"
        "}"
    );

    connect(m_btnClose.get(), &QPushButton::clicked,
            this, [=](){
        this->hide();
    });
    connect(m_btnLeft.get(), &QPushButton::clicked,
            this, [=](){
        EventBus::instance().publish("/video/videoplay/prev", "none");
    });
    connect(m_btnRight.get(), &QPushButton::clicked,
            this, [=](){
        EventBus::instance().publish("/video/videoplay/next", "none");
    });
}
void VideoPlaybackWidget::initLayout()
{
    /* 左侧：垂直居中的 左按钮 */
    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->addStretch();
    leftLayout->addWidget(m_btnLeft.get());
    leftLayout->addStretch();

    /* 右侧：垂直居中的 右按钮 */
    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addStretch();
    rightLayout->addWidget(m_btnRight.get());
    rightLayout->addStretch();

    /* 中间占位（你可以以后塞内容） */
    QWidget *centerWidget = new QWidget(this);

    /* 主横向布局 */
    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->addLayout(leftLayout);
    mainLayout->addWidget(centerWidget, 1);
    mainLayout->addLayout(rightLayout);

    /* 右上角关闭按钮 */
    QHBoxLayout *topRightLayout = new QHBoxLayout;
    topRightLayout->setContentsMargins(0, 0, 0, 0);
    topRightLayout->addStretch();        // 左边空白
    topRightLayout->addWidget(m_btnClose.get()); // 右边按钮

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addLayout(topRightLayout);
    rootLayout->addLayout(mainLayout, 1);

    setLayout(rootLayout);
}
void VideoPlaybackWidget::create(){
    video_play.create(1024, 592, 4);
    image_display.create(dmaBuf_render.get());
    video_play.add_video(&image_display);

    impl_device = std::make_shared<ImplDevice>();
    impl_device->subscribe("/video/videoplay/decode");
    impl_device->instance = this;
}
void VideoPlaybackWidget::ImplDevice::onMessage(const EventMsg& msg){
    JsonWrapper js(msg.payload);
    std::string filename;
    if(js.get("filename", filename)){
        LOG_DEBUG("video play widget", filename);
        if(instance){
            fs::path p(filename);
            if( p.extension() == ".jpg" || p.extension() == ".mp4"){
                instance->video_play.decode(p.string());
            }
        }
    }
}
bool VideoPlaybackWidget::decode(const std::string& filename){
    if(1){
        filename_lable->setText(QString::fromStdString(filename));
        filename_lable->adjustSize();
        return video_play.decode(filename);
    }
    return false;
}

void VideoPlaybackWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
            return;  // 忽略长按重复

    LOG_DEBUG("按键按下", event->key());

    if(event->key() == Qt::Key_VolumeUp){
        this->hide();
    }else if (event->key() == Qt::Key_VolumeDown){
        EventBus::instance().publish("/video/videoplay/prev", "none");
    }else if(event->key() == Qt::Key_MenuKB){
        EventBus::instance().publish("/video/videoplay/next", "none");
    }else if(event->key() == Qt::Key_Back){

    }
}

void VideoPlaybackWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
            return;  // 忽略长按重复

    LOG_DEBUG("按键释放", event->key());
}
void VideoPlaybackWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    this->setFocus();   // 显示时抢焦点
}

void VideoPlaybackWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);

    if (parentWidget())
        parentWidget()->setFocus();  // 隐藏时还焦点
}
