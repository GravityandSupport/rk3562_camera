#ifndef MP4_DEMUXER_H
#define MP4_DEMUXER_H

#include "videobase.h"
#include "ThreadSafeBoundedQueue.h"
#include "safe_thread.h"
#include "drmdumbbuffer.h"


extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}

class MP4_Demuxer : public VideoBase
{
public:
    bool open(const std::string& input_path);

    uint32_t width()       const { return width_; }
    uint32_t height()      const { return height_; }
    int64_t  frame_count() const { return frame_count_; }

    // 打印流信息（调试用）
    void dump_info() const ;

    MP4_Demuxer();
    virtual ~MP4_Demuxer();
private:
    std::string       input_path_;
    AVFormatContext*  fmt_ctx_;
    AVBSFContext*     bsf_ctx_;
    AVPacket*         pkt_;
    int               video_stream_idx_;
    uint32_t          width_;
    uint32_t          height_;
    int64_t           frame_count_;

    void init();
    void close();
    bool init_bsf();
    std::vector<VideoFramePtr> convert_packet(AVPacket* pkt);
    VideoFramePtr make_video_frame(AVPacket* pkt);
    std::vector<VideoFramePtr> flush_bsf();

    static void log_error(const char* func, int err) ;

    SafeThread thread_;
};

#endif // MP4_DEMUXER_H
