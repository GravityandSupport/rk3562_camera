#pragma once

#include "drmdumbbuffer.h"
#include "v4l2camera.h"
#include "refarray.h"
#include "videobase.h"

#include "PoolBuffer.h"
#include "rgacontrol.h"

class VideoMerge :  public VideoBase
{
public:
    struct Node{
        const VideoBase* video = nullptr;
        bool status = false;

        uint32_t x, y;
        uint32_t width_, height_;
        DrmDumbBuffer drm_buffer_;

        im_rect rect;
    };
    Node big_source, small_source;

    void create(uint32_t width, uint32_t height,
                const std::string& drm_dev = "/dev/dri/card0");

    VideoMerge(): process_queue(10){}
    virtual ~VideoMerge() = default;
protected:
    virtual void process_frames(VideoDrmBufPtr frame) override;
private:
    uint32_t width_, height_;
    std::string drm_dev_;

    std::condition_variable cv_;
    std::mutex      mutex_;

    SafeThread thread_;
    PoolBuffer<DrmDumbBuffer, 10> pool_buffer;

    ThreadSafeBoundedQueue<VideoDrmBufPtr> process_queue;
};

