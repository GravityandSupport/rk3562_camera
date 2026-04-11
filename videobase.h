#ifndef VIDEOBASE_H
#define VIDEOBASE_H

#include <QObject>
#include <vector>
#include <memory>

class VideoBase;

struct NV12_Packet{
    VideoBase* capture;
    int idx;
};

class VideoBase : public QObject
{
    Q_OBJECT
public:
    explicit VideoBase(QObject *parent = nullptr);
    virtual ~VideoBase() = default;

    virtual void process_frames(VideoBase* capture, int idx){(void)capture;(void)idx;}
    virtual void frames_ready(VideoBase* capture, int idx);

    using VideoFramePtr = std::shared_ptr<std::vector<uint8_t>>;
    virtual void process_frames(VideoFramePtr frame){(void)frame;}
    virtual void frames_ready(VideoFramePtr frame);
    /*        子类实现的具体处理逻辑
    virtual void process_frames(VideoFramePtr frame) {
        if (!frame || frame->empty()) return;

        // 访问数据的方式：
        // uint8_t* raw_data = frame->data();
        // size_t size = frame->size();
    }
     */

    void add_video(VideoBase* _video);
signals:

private:
    std::list<VideoBase*> video_list;
};

#endif // VIDEOBASE_H
