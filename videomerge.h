#pragma once

#include "drmdumbbuffer.h"
#include "v4l2camera.h"
#include "refarray.h"
#include "videobase.h"

#include "RingBuffer.h"
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

    DrmDumbBuffer drm_buffer_;

    void create(uint32_t width, uint32_t height,
                const std::string& drm_dev = "/dev/dri/card0");

    VideoMerge() = default;
    virtual ~VideoMerge() = default;
protected:
    virtual void process_frames(VideoDrmBufPtr frame) override;
private:
    uint32_t width_, height_;
    std::string drm_dev_;

    std::mutex      mutex_;
};

