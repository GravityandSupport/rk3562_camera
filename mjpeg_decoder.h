#pragma once

#include "rockchip/rk_mpi.h"
#include "rockchip/mpp_log.h"

#include "drmdumbbuffer.h"
#include "videobase.h"
#include <memory>
#include <vector>

class MJPEG_Decoder : public VideoBase
{
public:
    void create(int width, int height, uint32_t buffer_num);

    bool decode_frame(uint8_t* data, size_t length);

    MJPEG_Decoder();
    virtual ~MJPEG_Decoder();
protected:
    virtual void process_frames(VideoDrmBufPtr frame) override;
private:
    int m_width;
    int m_height;
    uint32_t buffer_num_;

    MppCtx ctx = nullptr;
    MppApi* mpi = nullptr;
    MppBufferGroup group = nullptr;

    std::vector<std::shared_ptr<DrmDumbBuffer>> drm_buf;

    bool find_jpg_end_marker(const uint8_t* data, size_t length);
};