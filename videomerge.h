#pragma once

#include "drmdumbbuffer.h"
#include "v4l2camera.h"
#include "refarray.h"
#include "videobase.h"

#include "RingBuffer.h"

class VideoMerge :  public VideoBase
{
public:
    struct Node{
        const VideoBase* video = nullptr;
        bool status = false;
    };
    Node big_source, small_source;

    RingBuffer<std::shared_ptr<DrmDumbBuffer>, 10> ring_buffer;

    void create(uint32_t width, uint32_t height,
                const std::string& drm_dev = "/dev/dri/card0");

    VideoMerge() = default;
    virtual ~VideoMerge() = default;
protected:
    virtual void process_frames(VideoBase* capture, int idx) override;
private:
    uint32_t width_, height_;
    std::string drm_dev_;
};

