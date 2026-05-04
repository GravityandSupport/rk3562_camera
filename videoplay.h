#ifndef VIDEOPLAY_H
#define VIDEOPLAY_H

#include "drmdumbbuffer.h"
#include "v4l2camera.h"
#include "refarray.h"
#include "videobase.h"

#include "PoolBuffer.h"
#include "rgacontrol.h"
#include <memory>
#include "mjpeg_decoder.h"
#include "DmaBufRenderer.h"
#include "ImageDisplay.h"
#include "h264_decoder.h"
#include "mp4_demuxer.h"

class VideoPlay : public VideoBase
{
public:
    void create(int width, int height, uint32_t buffer_num);

    bool decode_jpeg(const std::string& filename);
    bool decode_mp4(const std::string& filename);

    VideoPlay();
protected:
    virtual void process_frames(VideoDrmBufPtr frame) override;
private:
    int m_width;
    int m_height;
    uint32_t buffer_num_;

    std::mutex      mutex_;

    PoolBuffer<DrmDumbBuffer, 4> pool_buffer;

    MJPEG_Decoder mjpeg_decoder;
    H264_Decoder h264_decoder;
    MP4_Demuxer mp4_demuxer;
};

#endif // VIDEOPLAY_H
