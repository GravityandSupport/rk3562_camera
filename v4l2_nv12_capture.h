#ifndef V4L2_NV12_CAPTURE_H
#define V4L2_NV12_CAPTURE_H

#include <memory>
#include <unordered_map>

#include "drmdumbbuffer.h"
#include "v4l2camera.h"
#include "refarray.h"
#include "videobase.h"

#include "safe_thread.h"
#include "timermanager.h"
#include "array"

#include "PoolBuffer.h"

class V4L2_NV12_Capture :  public VideoBase
{
public:
    V4L2_NV12_Capture() = default;

    int create(uint32_t width, uint32_t height,
                uint32_t buffer_num,
                const std::string& video_dev,
                const std::string& drm_dev = "/dev/dri/card0");

    int register_device();
    int start_stream();
    void stop_stream();
    void uninstall_device();

    int queue_dmabuf(uint32_t idx);
    DrmDumbBuffer* acquire_dmabuf(uint32_t idx);
    void release_dmabuf(uint32_t idx);

private:
    uint32_t width_, height_;
    uint32_t buffer_num_;
    std::string video_dev_, drm_dev_;

    V4L2Camera cam;

    std::vector<DrmDumbBuffer*> drm_buffers;
//    RefArray ref_array_manage;

    SafeThread dequeue_thread;
};

#endif // V4L2_NV12_CAPTURE_H
