#ifndef H264_NALUSAVE_H
#define H264_NALUSAVE_H

#include "videobase.h"
#include "ThreadSafeBoundedQueue.h"
#include "safe_thread.h"
#include "drmdumbbuffer.h"
#include "EventBus.h"

class H264_Encoder;

class H264_NaluSave : public VideoBase
{
public:
    void create(H264_Encoder* input_source);

    bool save(const std::string& name);

    H264_NaluSave();
protected:
    virtual void process_frames(VideoFramePtr frame) override;

private:
    H264_Encoder* input_source_ = nullptr;

    SafeThread thread_;
    ThreadSafeBoundedQueue<VideoFramePtr> process_queue;

    std::mutex mutex_;

    ThreadSafeBoundedQueue<std::string> file_queue;
};

#endif // H264_NALUSAVE_H
