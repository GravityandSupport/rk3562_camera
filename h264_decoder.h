#ifndef H264_DECODER_H
#define H264_DECODER_H

#include "ThreadSafeBoundedQueue.h"
#include "safe_thread.h"
#include "drmdumbbuffer.h"
#include "videobase.h"
#include "rockchip/rk_mpi.h"

class H264_Decoder : public VideoBase
{
public:
    void create(int width, int height, uint32_t buffer_num);

    bool decode_frame(uint8_t* data, size_t length);

    H264_Decoder();
    virtual ~H264_Decoder();
protected:
    virtual void process_frames(VideoFramePtr frame) override;
private:
    int m_width;
    int m_height;
    uint32_t buffer_num_;

    /* ---------- MPP ---------- */
    MppCtx          m_mppCtx=nullptr;
    MppApi         *m_mppApi=nullptr;
    MppBufferGroup group = nullptr;

    std::vector<std::shared_ptr<DrmDumbBuffer>> drm_buf;

    SafeThread thread_;
    ThreadSafeBoundedQueue<VideoFramePtr> process_queue;
};

#endif // H264_DECODER_H
