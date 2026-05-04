#ifndef VIDEOPLAYBACKWIDGET_H
#define VIDEOPLAYBACKWIDGET_H

#include <QWidget>
#include <QPushButton>
#include "DmaBufRenderer.h"
#include "ImageDisplay.h"
#include "videoplay.h"
#include "event_device.h"
#include <QKeyEvent>
#include <QLabel>

class VideoPlaybackWidget : public QWidget
{
    Q_OBJECT
public:
    void create();

    bool decode(const std::string& filename);

    explicit VideoPlaybackWidget(QWidget *parent = nullptr);
protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
signals:

private:
    class ImplDevice : public EventDevice{
    public:
        VideoPlaybackWidget* instance = nullptr;
        virtual void onMessage(const EventMsg& msg) override;
    };
    std::shared_ptr<ImplDevice> impl_device;

    std::shared_ptr<DmaBufRenderer> dmaBuf_render;
    ImageDisplay image_display;

    VideoPlay video_play;

    std::shared_ptr<QPushButton> m_btnLeft;
    std::shared_ptr<QPushButton> m_btnRight;
    std::shared_ptr<QPushButton> m_btnClose;
//    std::shared_ptr<QPushButton> m_btnPause;

    void initUi();
    void initLayout();

    QLabel*         filename_lable;
};

#endif // VIDEOPLAYBACKWIDGET_H
