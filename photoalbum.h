#ifndef PHOTOALBUM_H
#define PHOTOALBUM_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QKeyEvent>
#include "videobase.h"
#include "ThreadSafeBoundedQueue.h"
#include "safe_thread.h"
#include "drmdumbbuffer.h"
#include "EventBus.h"
#include "qteventdevice.h"
#include "mjpeg_decoder.h"
#include "videoplaybackwidget.h"

class PhotoAlbum : public QWidget
{
    Q_OBJECT
public:
    void create();

    void loadPathFile(const std::string& path);


    explicit PhotoAlbum(const std::string& path, QWidget *parent = nullptr);
protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
private:
    std::string path_;

//    std::atomic<std::string> load_path;
    std::string load_path;
    std::atomic_bool quit_ = false;

    QIcon folder_icon_;
    QIcon mp4_icon_;

    class ImplDevice : public QtEventDevice{
    public:
        PhotoAlbum* instance = nullptr;
    };
    std::shared_ptr<ImplDevice> impl_device;

    SafeThread thread_;

    std::shared_ptr<QListWidget> listWidget;
    std::shared_ptr<QPushButton> backButton;
    std::shared_ptr<VideoPlaybackWidget> video_play_widget;

    void onItemActivated(QListWidgetItem *item);
    void onBackButtonClick();

    bool isValidMedia(const QString& path);
signals:

};

#endif // PHOTOALBUM_H
