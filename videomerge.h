#pragma once

#include "drmdumbbuffer.h"
#include "v4l2camera.h"
#include "refarray.h"
#include "videobase.h"

#include "RingBuffer.h"

class VideoMerge :  public VideoBase
{
public:
    RingBuffer<std::shared_ptr<DrmDumbBuffer>, 10> ring_buffer;

    void create(uint32_t width, uint32_t height,
                uint32_t buffer_num,
                const std::string& drm_dev = "/dev/dri/card0");

    VideoMerge() = default;
    virtual ~VideoMerge() = default;
protected:
    virtual void process_frames(VideoBase* capture, int idx) override;
private:
    uint32_t width_, height_;
    uint32_t buffer_num_;
    std::string drm_dev_;
};

