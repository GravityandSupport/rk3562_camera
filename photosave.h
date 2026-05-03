#ifndef PHOTOSAVE_H
#define PHOTOSAVE_H

#include "videobase.h"
#include "ThreadSafeBoundedQueue.h"
#include "safe_thread.h"
#include "drmdumbbuffer.h"

class MJPEG_Encoder;

class PhotoSave : public VideoBase
{
public:
    void create(MJPEG_Encoder* input_source);

    bool save(const std::string name);

    static bool saveJPEGToFile(const void* data, size_t size, const std::string& filename);

    PhotoSave();

protected:
    virtual void process_frames(VideoFramePtr frame) override;
private:
    MJPEG_Encoder* input_source_ = nullptr;

    SafeThread thread_;
    ThreadSafeBoundedQueue<VideoFramePtr> process_queue;

    std::mutex mutex_;

    ThreadSafeBoundedQueue<std::string> file_queue;
};

#endif // PHOTOSAVE_H
