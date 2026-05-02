#include "photoalbum.h"
#include <QVBoxLayout>
#include <experimental/filesystem>
#include "JsonWrapper.h"
#include <qdebug.h>
#include "outLog.h"
#include <forward_list>
#include <QMessageBox>

namespace fs = std::experimental::filesystem;

PhotoAlbum::PhotoAlbum(const std::string& path, QWidget *parent) : QWidget(parent), path_(path), load_path(path)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    backButton = std::make_shared<QPushButton>("返回", this);
    backButton->setFixedHeight(40);

    listWidget = std::make_shared<QListWidget>(this);
    // 相册设置
    listWidget->setViewMode(QListView::IconMode);
    listWidget->setResizeMode(QListView::Adjust);
    listWidget->setMovement(QListView::Static);

    listWidget->setIconSize(QSize(160, 120));
    listWidget->setGridSize(QSize(180, 140));
    listWidget->setSpacing(10);
    listWidget->setFrameShape(QFrame::NoFrame);

    listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    listWidget->setFrameShape(QFrame::NoFrame);
    listWidget->setFocusPolicy(Qt::NoFocus);

    layout->addWidget(backButton.get());
    layout->addWidget(listWidget.get());

    connect(backButton.get(), &QPushButton::clicked,
            this, [=](){
        if(load_path==path){
            this->hide();
        }else{
            fs::path p(load_path);
            load_path = p.parent_path().string();
            loadPathFile(load_path);
        }
    });

    connect(listWidget.get(), &QListWidget::itemDoubleClicked,
            this, [=](QListWidgetItem *item) {

        std::string path = item->data(Qt::UserRole).toString().toStdString();
        LOG_DEBUG("点击文件路径", path);

        fs::path p(path);
        if(fs::is_directory(p)){
            loadPathFile(path);
        }
    });
}

void PhotoAlbum::create(){
    impl_device = std::make_shared<ImplDevice>();
    impl_device->subscribe("/video/videoplay/create");
    impl_device->instance = this;
    connect(impl_device.get(), &ImplDevice::messageReceived, this, [&](const QString& topic, const QString& payload){
//        qDebug() << topic << ", " << payload << "\n";
//        LOG_DEBUG("photo album", topic.toStdString(), payload.toStdString());
        (void)(topic);

        JsonWrapper js(payload.toStdString());
        std::string path;
        if(js.get("filename", path)){
            LOG_DEBUG("photo album", path);
            QListWidgetItem *item = new QListWidgetItem;

            fs::path p(path);
            if(fs::is_directory(p)){
                item->setIcon(QIcon("/mnt/nfs_dir/res/ui/folder.png"));
            }else if(p.extension() == ".jpg"){
                item->setIcon(QIcon(QString::fromStdString(p.string())));
            }else{
                item->setIcon(QIcon("/mnt/nfs_dir/res/ui/unkowntype.png"));
            }

            item->setText(QString::fromStdString(p.filename().string()));
            item->setTextAlignment(Qt::AlignHCenter); // 文字居中（非常重要）
            item->setSizeHint(QSize(180, 140));
            item->setData(Qt::UserRole, QString::fromStdString(p.string()));
            listWidget->addItem(item);
        }

    });

    thread_.set_loop_callback([this](SafeThread* self) ->bool{
        (void)self;
        fs::path p(load_path);

        try {
            if (!fs::exists(p)){ LOG_DEBUG("photo album", "路径不存在", load_path); return false;}

            std::forward_list<std::string> direct_list, file_list;

            for(const auto& entry : fs::directory_iterator(p)){
                if(fs::is_directory(entry)){
                    direct_list.push_front(entry.path().string());
                }else{
                    file_list.push_front(entry.path().string());
                }
                if(quit_.load(std::memory_order_relaxed)==true){return false;}
            }

            direct_list.sort();
            file_list.sort();

            for(auto& n : direct_list){
                JsonWrapper js;
                js.import("filename", n);
                impl_device->publish("/video/videoplay/create", js.dump());
                SafeThread::msDelay(50);
                if(quit_.load(std::memory_order_relaxed)==true){return false;}
            }
            for(auto& n : file_list){
                JsonWrapper js;
                js.import("filename", n);
                impl_device->publish("/video/videoplay/create", js.dump());
                SafeThread::msDelay(30);
                if(quit_.load(std::memory_order_relaxed)==true){return false;}
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "文件系统错误: " << e.what() << std::endl;
        }


        return false;
    });
}

void PhotoAlbum::loadPathFile(const std::string& path){
    quit_.store(true, std::memory_order_relaxed);
    thread_.stop();
    quit_.store(false, std::memory_order_relaxed);
    load_path = path;
    listWidget->clear();                     // 删除所有 item
    thread_.start("photo album");
}
void PhotoAlbum::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
            return;  // 忽略长按重复

    LOG_DEBUG("按键按下", event->key());

    if (event->key() == Qt::Key_VolumeDown) {

    }
}

void PhotoAlbum::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
            return;  // 忽略长按重复

    LOG_DEBUG("按键释放", event->key());
}
void PhotoAlbum::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    this->setFocus();   // 显示时抢焦点
}

void PhotoAlbum::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);

    if (parentWidget())
        parentWidget()->setFocus();  // 隐藏时还焦点
}
