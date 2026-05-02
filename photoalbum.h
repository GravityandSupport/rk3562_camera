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

    void showEvent(QShowEvent *event);
    void hideEvent(QHideEvent *event);
private:
    std::string path_;

//    std::atomic<std::string> load_path;
    std::string load_path;
    std::atomic_bool quit_ = false;

    class ImplDevice : public QtEventDevice{
    public:
        PhotoAlbum* instance = nullptr;
    };
    std::shared_ptr<ImplDevice> impl_device;

    SafeThread thread_;

    std::shared_ptr<QListWidget> listWidget;
    std::shared_ptr<QPushButton> backButton;
signals:

};

#endif // PHOTOALBUM_H
