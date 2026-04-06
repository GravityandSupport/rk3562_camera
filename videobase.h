#ifndef VIDEOBASE_H
#define VIDEOBASE_H

#include <QObject>

class VideoBase : public QObject
{
    Q_OBJECT
public:
    explicit VideoBase(QObject *parent = nullptr);
    virtual ~VideoBase() = default;

    virtual void process_frames(VideoBase* capture, int idx){(void)capture;(void)idx;}

    virtual void frames_ready(VideoBase* capture, int idx);

    void add_video(VideoBase* _video);
signals:

private:
    std::list<VideoBase*> video_list;
};

#endif // VIDEOBASE_H
