#include "mp4_demuxer.h"
#include "outLog.h"
MP4_Demuxer::MP4_Demuxer()
{
    init();
    thread_.set_start_callback([this](SafeThread* self)->bool{
        (void)self;
        if (!fmt_ctx_ || video_stream_idx_ < 0) {
            std::cerr << "[MP4Demuxer] Not opened." << std::endl;
            return false;
        }

        pkt_ = av_packet_alloc();
        if(!pkt_){
           std::cerr << "[MP4Demuxer] av_packet_alloc failed." << std::endl;
           return false;
        }
        return true;
    });
    thread_.set_loop_callback([this](SafeThread* self) ->bool{
        (void)self;
        int ret = av_read_frame(fmt_ctx_, pkt_);
        if (ret == AVERROR_EOF) {
            return false;
        }
        if (ret < 0) {
            log_error("av_read_frame", ret);
            return false;
        }

        if (pkt_->stream_index != video_stream_idx_) {
            av_packet_unref(pkt_);
            return true;
        }

        // 通过 BSF 转换为 Annex-B
        auto frames = convert_packet(pkt_);
        av_packet_unref(pkt_);

        for(auto& f : frames){
            frames_ready(f);
            ++frame_count_;
        }

        LOG_DEBUG("mp4 demuxer", frame_count_);
        self->msDelay(30);
        return true;
    });
    thread_.set_end_callback([this](SafeThread* self)->bool{
        (void)self;
        auto flushed = flush_bsf();
        for (auto& f : flushed) {
            frames_ready(f);
            ++frame_count_;
        }

        av_packet_free(&pkt_);
        return true;
    });
}
MP4_Demuxer::~MP4_Demuxer(){
    close();
}
void MP4_Demuxer::dump_info() const {
    if (fmt_ctx_) av_dump_format(fmt_ctx_, 0, input_path_.c_str(), 0);
}
void MP4_Demuxer::init(){
    fmt_ctx_=nullptr;
    bsf_ctx_=nullptr;
    video_stream_idx_=-1;
    width_=0;
    height_=0;
    frame_count_=0;
}
void MP4_Demuxer::close() {
    if (bsf_ctx_) {
        av_bsf_free(&bsf_ctx_);
        bsf_ctx_ = nullptr;
    }
    if (fmt_ctx_) {
        avformat_close_input(&fmt_ctx_);
        fmt_ctx_ = nullptr;
    }
    video_stream_idx_ = -1;
}
bool MP4_Demuxer::open(const std::string& input_path){
    thread_.stop();
    close();
    init();
    input_path_ = input_path;

    int ret;

    // 打开输入文件
    ret = avformat_open_input(&fmt_ctx_, input_path_.c_str(), nullptr, nullptr);
    if (ret < 0) {
        log_error("avformat_open_input", ret);
        return false;
    }

    // 读取流信息
    ret = avformat_find_stream_info(fmt_ctx_, nullptr);
    if (ret < 0) {
        log_error("avformat_find_stream_info", ret);
        return false;
    }

    // 找到第一条 H264 视频流
    for (unsigned i = 0; i < fmt_ctx_->nb_streams; ++i) {
        AVStream* st = fmt_ctx_->streams[i];
        if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
            st->codecpar->codec_id  == AV_CODEC_ID_H264) {
            video_stream_idx_ = (int)i;
            width_  = (uint32_t)st->codecpar->width;
            height_ = (uint32_t)st->codecpar->height;
            break;
        }
    }

    if (video_stream_idx_ < 0) {
        std::cerr << "[MP4Demuxer] No H264 video stream found in: "
                  << input_path_ << std::endl;
        return false;
    }

    // 挂载 BSF：mp4 内部是 AVCC（长度前缀），需要转成 Annex-B
    if (!init_bsf()) return false;

    LOG_DEBUG("DEBUG");
    thread_.start("mp4 demuxer");

    std::cout << "[MP4Demuxer] Opened: " << input_path_
              << "  resolution=" << width_ << "x" << height_
              << "  stream_idx=" << video_stream_idx_ << std::endl;
    return true;
}


// 初始化 h264_mp4toannexb BSF
bool MP4_Demuxer::init_bsf() {
    const AVBitStreamFilter* bsf = av_bsf_get_by_name("h264_mp4toannexb");
    if (!bsf) {
        std::cerr << "[MP4Demuxer] BSF h264_mp4toannexb not found." << std::endl;
        return false;
    }

    int ret = av_bsf_alloc(bsf, &bsf_ctx_);
    if (ret < 0) { log_error("av_bsf_alloc", ret); return false; }

    // 把视频流的 codec 参数复制进 BSF
    AVStream* st = fmt_ctx_->streams[video_stream_idx_];
    ret = avcodec_parameters_copy(bsf_ctx_->par_in, st->codecpar);
    if (ret < 0) { log_error("avcodec_parameters_copy", ret); return false; }

    bsf_ctx_->time_base_in = st->time_base;

    ret = av_bsf_init(bsf_ctx_);
    if (ret < 0) { log_error("av_bsf_init", ret); return false; }

    return true;
}
// 将一个 AVCC packet 送入 BSF，取出所有 Annex-B 帧
std::vector<MP4_Demuxer::VideoFramePtr> MP4_Demuxer::convert_packet(AVPacket* pkt) {
    std::vector<VideoFramePtr> result;

    int ret = av_bsf_send_packet(bsf_ctx_, pkt);
    if (ret < 0) {
        log_error("av_bsf_send_packet", ret);
        return result;
    }

    AVPacket* out_pkt = av_packet_alloc();
    if (!out_pkt) return result;

    while (true) {
        ret = av_bsf_receive_packet(bsf_ctx_, out_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) {
            log_error("av_bsf_receive_packet", ret);
            break;
        }

        auto frame = make_video_frame(out_pkt);
        if (frame) result.push_back(frame);

        av_packet_unref(out_pkt);
    }

    av_packet_free(&out_pkt);
    return result;
}
MP4_Demuxer::VideoFramePtr MP4_Demuxer::make_video_frame(AVPacket* pkt) {
    if (!pkt || pkt->size <= 0) return nullptr;

    auto frame  = std::make_shared<VideoFrame>();
    frame->width  = width_;
    frame->height = height_;
    frame->stride = width_;   // 裸流没有行对齐概念，stride == width
    frame->data   = std::make_shared<std::vector<uint8_t>>(
                        pkt->data, pkt->data + pkt->size);
    return frame;
}
std::vector<MP4_Demuxer::VideoFramePtr> MP4_Demuxer::flush_bsf() {
    std::vector<VideoFramePtr> result;
    if (!bsf_ctx_) return result;

    int ret = av_bsf_send_packet(bsf_ctx_, nullptr); // nullptr = flush
    if (ret < 0) return result;

    AVPacket* out_pkt = av_packet_alloc();
    if (!out_pkt) return result;

    while (true) {
        ret = av_bsf_receive_packet(bsf_ctx_, out_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) break;

        auto frame = make_video_frame(out_pkt);
        if (frame) result.push_back(frame);
        av_packet_unref(out_pkt);
    }

    av_packet_free(&out_pkt);
    return result;
}
void MP4_Demuxer::log_error(const char* func, int err) {
    char buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err, buf, sizeof(buf));
    std::cerr << "[MP4Demuxer] " << func << " failed: " << buf << std::endl;
}
