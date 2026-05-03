#ifndef MP4_MUXER_H
#define MP4_MUXER_H

#include "videobase.h"
#include "ThreadSafeBoundedQueue.h"
#include "safe_thread.h"
#include "drmdumbbuffer.h"
#include "EventBus.h"


extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}


class H264_Encoder;

class MP4_Muxer : public VideoBase
{
public:
    void create(H264_Encoder* input_source);
    bool save(const std::string& name);
    void finish();

    bool isRunning() {return runnig_;}

    MP4_Muxer();
protected:
    virtual void process_frames(VideoFramePtr frame) override;

private:
    struct NalUnit {
        const uint8_t* data;
        size_t size;
        uint8_t type;
    };

    H264_Encoder* input_source_ = nullptr;

    SafeThread thread_;
    ThreadSafeBoundedQueue<VideoFramePtr> process_queue;

    std::mutex mutex_;

    bool runnig_ = false;

    // ============================
    std::string output_path_;
    AVFormatContext* fmt_ctx_;
    AVStream* video_stream_;
    AVCodecContext* codec_ctx_;

    std::vector<uint8_t> sps_;
    std::vector<uint8_t> pps_;

    int64_t frame_count_;
    int64_t pts_;
    bool initialized_;
    bool header_written_;

    bool extract_and_init(VideoFramePtr frame);
    bool init_muxer(uint32_t width, uint32_t height);
    bool build_extradata();
    std::vector<uint8_t> annexb_to_avcc(const uint8_t* data, size_t size);
    bool write_frame(VideoFramePtr frame);
    std::vector<NalUnit> parse_nal_units(const uint8_t* data, size_t size);
    std::vector<NalUnit> parse_nal_units_optimized(const uint8_t* data, size_t size);
};

#endif // MP4_MUXER_H
