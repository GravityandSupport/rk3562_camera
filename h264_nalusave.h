#ifndef H264_NALUSAVE_H
#define H264_NALUSAVE_H

#include "videobase.h"
#include "ThreadSafeBoundedQueue.h"
#include "safe_thread.h"
#include "drmdumbbuffer.h"
#include "EventBus.h"

class H264_NaluSave : public VideoBase
{
public:
    void create();

    H264_NaluSave();
protected:
    virtual void process_frames(VideoFramePtr frame) override;

private:
    class ImplDevice : public EventDevice{
    public:
        H264_NaluSave* instance = nullptr;
        virtual void onMessage(const EventMsg& msg) override;
    };
    std::shared_ptr<ImplDevice> impl_device;

    friend ImplDevice;

    SafeThread thread_;
    ThreadSafeBoundedQueue<VideoFramePtr> process_queue;

    std::mutex mutex_;

    ThreadSafeBoundedQueue<std::string> file_queue;
};

#endif // H264_NALUSAVE_H
