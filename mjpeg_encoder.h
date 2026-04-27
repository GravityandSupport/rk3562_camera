#pragma once

#include "rockchip/rk_mpi.h"
#include "rockchip/mpp_log.h"

#include "drmdumbbuffer.h"
#include "videobase.h"
#include <memory>
#include <vector>

class MJPEG_Encoder : public VideoBase
{
public:
    void create(int width, int height, uint32_t buffer_num);

    bool encode_frame(DrmDumbBuffer* drm_buf);

    MJPEG_Encoder();
    virtual ~MJPEG_Encoder();
protected:
    void process_frames(VideoDrmBufPtr frame);
private:
    int m_width;
    int m_height;
    uint32_t buffer_num_;

    MppCtx ctx = nullptr;
    MppApi* mpi = nullptr;
    MppEncCfg cfg = nullptr;
    MppBufferGroup group = nullptr;

//    std::vector<std::shared_ptr<DrmDumbBuffer>> drm_buf;
};

