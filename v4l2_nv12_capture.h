#ifndef V4L2_NV12_CAPTURE_H
#define V4L2_NV12_CAPTURE_H

#include <memory>

#include "wyrThread.hpp"
#include "drmdumbbuffer.h"
#include "v4l2camera.h"
#include "refarray.h"
#include "videobase.h"

class V4L2_NV12_Capture : public __Thread_t , public VideoBase
{
public:
    V4L2_NV12_Capture() = default;
    V4L2_NV12_Capture(uint32_t width, uint32_t height,
            uint32_t buffer_num,
            const std::string& video_dev,
            const std::string& drm_dev = "/dev/dri/card0")
            : width_(width), height_(height),
                buffer_num_(buffer_num),
                video_dev_(video_dev), drm_dev_(drm_dev), cam(std::make_shared<V4L2Camera>(buffer_num)) {}

    int register_device();
    int start_stream();
    void stop_stream();
    void uninstall_device();

    int queue_dmabuf(uint32_t idx);
    DrmDumbBuffer* acquire_dmabuf(uint32_t idx);
    void release_dmabuf(uint32_t idx);
protected:
    virtual bool threadLoop()  override;
private:
    uint32_t width_, height_;
    uint32_t buffer_num_;
    std::string video_dev_, drm_dev_;

    std::shared_ptr<V4L2Camera> cam;

    std::vector<DrmDumbBuffer*> drm_buffers;
    RefArray ref_array_manage;
};

#endif // V4L2_NV12_CAPTURE_H
