#ifndef H264_ENCODER_H
#define H264_ENCODER_H

#include "videobase.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "ThreadSafeBoundedQueue.h"
#include "safe_thread.h"
#include "drmdumbbuffer.h"

#include "rockchip/rk_mpi.h"

class H264_Encoder : public VideoBase
{
public:
    enum class EncodeStatus{
        Stop, Start
    };
    EncodeStatus encode_status=EncodeStatus::Stop;

    H264_Encoder();
    virtual ~H264_Encoder() = default;

    virtual void process_frames(VideoDrmBufPtr frame) override;

    bool start_encoder(int width, int height, int fps);
    bool stop_encoder();
private:
    bool initMPP();
    bool encodeFrame(const DrmDumbBuffer* input);

    int m_width;
    int m_height;
    int m_fps;

    /* ---------- MPP ---------- */
    MppCtx          m_mppCtx=nullptr;
    MppApi         *m_mppApi=nullptr;
    MppEncCfg       m_encCfg=nullptr;

    uint32_t frame_id=0;
};

#endif // H264_ENCODER_H
