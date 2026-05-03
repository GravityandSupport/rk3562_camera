#include "mp4_muxer.h"
#include "outLog.h"
#include "h264_encoder.h"

/*
nalu type: 7 8 6 6 5
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 7 8 6 6 5
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
nalu type: 1
*/

MP4_Muxer::MP4_Muxer()  : process_queue(3)
{
    thread_.set_loop_callback([this](SafeThread* self) ->bool{
        (void)self;

        VideoFramePtr frame;
        if(process_queue.timed_pop(frame, 2000)){
            if (!initialized_){
                auto nals = parse_nal_units_optimized(frame->data->data(), frame->data->size());
                printf("nalu type: ");
                for(auto& nalu : nals){
                    printf("%d ", nalu.type);
                }printf("\n");
                LOG_DEBUG("debug");
                if (!extract_and_init(frame)){
                    // Wait for SPS/PPS
                    std::cerr << "Waiting for SPS/PPS..." << std::endl;
                    return true;
                }
            }
            return write_frame(frame);
        }

        return true;
    });
}
void MP4_Muxer::create(H264_Encoder* input_source){
    input_source_ = input_source;
}
void MP4_Muxer::process_frames(VideoFramePtr frame){
    process_queue.push(frame);
}
bool MP4_Muxer::save(const std::string& name){
    if(thread_.isRunning()) {return false;}
    bool ret = false;
    if(input_source_){input_source_->node_state.enable();}
    initialized_ = false;
    frame_count_ = 0;
    pts_ = 0;
    header_written_ = false;
    output_path_ = name;
    sps_.clear();
    pps_.clear();
    thread_.start();
    runnig_ = true;
    return ret;
}
void MP4_Muxer::finish(){
    thread_.stop();
    if(input_source_){input_source_->node_state.disable();}
    if (!initialized_) return ;

    if (header_written_ && fmt_ctx_) {
        av_write_trailer(fmt_ctx_);
    }

    if (fmt_ctx_) {
        if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&fmt_ctx_->pb);
        }
        avformat_free_context(fmt_ctx_);
        fmt_ctx_ = nullptr;
    }

    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }

    initialized_ = false;
    header_written_ = false;
    runnig_ = false;
}

bool MP4_Muxer::extract_and_init(VideoFramePtr frame) {
    const uint8_t* data = frame->data->data();
    size_t size = frame->data->size();

    auto nals = parse_nal_units_optimized(data, size);

    std::vector<uint8_t> sps_data, pps_data;

    for (auto& nal : nals) {
        if (nal.type == 7) { // SPS
            sps_data.assign(nal.data, nal.data + nal.size);
            sps_ = sps_data;
        } else if (nal.type == 8) { // PPS
            pps_data.assign(nal.data, nal.data + nal.size);
            pps_ = pps_data;
        }
    }

    if (sps_.empty() || pps_.empty()) {
        return false;
    }

    return init_muxer(frame->width, frame->height);
}
bool MP4_Muxer::init_muxer(uint32_t width, uint32_t height){
    int ret;

    // Allocate output context
    ret = avformat_alloc_output_context2(&fmt_ctx_, nullptr, nullptr, output_path_.c_str());
    if (ret < 0 || !fmt_ctx_) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        std::cerr << "Failed to allocate output context: " << err_buf << std::endl;
        return false;
    }

    // Find H264 encoder (we use it just for codec parameters)
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cerr << "H264 codec not found" << std::endl;
        return false;
    }

    // Add video stream
    video_stream_ = avformat_new_stream(fmt_ctx_, nullptr);
    if (!video_stream_) {
        std::cerr << "Failed to create video stream" << std::endl;
        return false;
    }

    // Set up codec parameters
    video_stream_->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    video_stream_->codecpar->codec_id = AV_CODEC_ID_H264;
    video_stream_->codecpar->width = width;
    video_stream_->codecpar->height = height;
    video_stream_->codecpar->format = AV_PIX_FMT_YUV420P;

    // Build extradata (AVCC format) from SPS/PPS
    if (!build_extradata()) {
        std::cerr << "Failed to build extradata" << std::endl;
        return false;
    }

    // Set timebase (assuming 30fps)
    video_stream_->time_base = {1, 30};
    video_stream_->avg_frame_rate = {30, 1};

    // Open output file
    if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&fmt_ctx_->pb, output_path_.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            char err_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, err_buf, sizeof(err_buf));
            std::cerr << "Failed to open output file: " << err_buf << std::endl;
            return false;
        }
    }

    // Write file header
    ret = avformat_write_header(fmt_ctx_, nullptr);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        std::cerr << "Failed to write header: " << err_buf << std::endl;
        return false;
    }

    header_written_ = true;
    initialized_ = true;
    std::cout << "MP4 muxer initialized: " << width << "x" << height << std::endl;

    return true;
}
bool MP4_Muxer::build_extradata() {
    if (sps_.empty() || pps_.empty()) return false;

    // AVCC format:
    // configurationVersion (1) + AVCProfileIndication (1) +
    // profile_compatibility (1) + AVCLevelIndication (1) +
    // lengthSizeMinusOne (1) + numSequenceParameterSets (1) +
    // [sps_length (2) + sps_data] +
    // numPictureParameterSets (1) +
    // [pps_length (2) + pps_data]

    size_t extra_size = 6 + 2 + sps_.size() + 1 + 2 + pps_.size();
    uint8_t* extra = (uint8_t*)av_malloc(extra_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!extra) return false;

    memset(extra, 0, extra_size + AV_INPUT_BUFFER_PADDING_SIZE);

    size_t pos = 0;
    extra[pos++] = 1;                    // configurationVersion
    extra[pos++] = sps_[1];             // AVCProfileIndication
    extra[pos++] = sps_[2];             // profile_compatibility
    extra[pos++] = sps_[3];             // AVCLevelIndication
    extra[pos++] = 0xFF;                // lengthSizeMinusOne = 3 (4 bytes)
    extra[pos++] = 0xE1;                // numSequenceParameterSets = 1

    // SPS
    extra[pos++] = (sps_.size() >> 8) & 0xFF;
    extra[pos++] = sps_.size() & 0xFF;
    memcpy(extra + pos, sps_.data(), sps_.size());
    pos += sps_.size();

    // PPS
    extra[pos++] = 1;                   // numPictureParameterSets
    extra[pos++] = (pps_.size() >> 8) & 0xFF;
    extra[pos++] = pps_.size() & 0xFF;
    memcpy(extra + pos, pps_.data(), pps_.size());
    pos += pps_.size();

    video_stream_->codecpar->extradata = extra;
    video_stream_->codecpar->extradata_size = (int)extra_size;

    return true;
}
std::vector<uint8_t> MP4_Muxer::annexb_to_avcc(const uint8_t* data, size_t size) {
    std::vector<uint8_t> avcc;

    // Skip SPS/PPS in frame data (already in extradata)
    // But include them if needed for keyframes
    uint32_t nal_size = (uint32_t)size;
    // Write 4-byte big-endian length
    avcc.push_back((nal_size >> 24) & 0xFF);
    avcc.push_back((nal_size >> 16) & 0xFF);
    avcc.push_back((nal_size >> 8) & 0xFF);
    avcc.push_back(nal_size & 0xFF);
    // Write NAL data
    avcc.insert(avcc.end(), data, data + size);

    return avcc;
}
bool MP4_Muxer::write_frame(VideoFramePtr frame){
    if (!initialized_) return false;

    const uint8_t* data = nullptr;
    size_t size = 0;

    // Check if this is a keyframe (contains IDR NAL unit type=5)
    auto nals = parse_nal_units_optimized(frame->data->data(), frame->data->size());
    bool is_keyframe = false;
    for (auto& nal : nals) {
        if(nal.type == 1){
            data = nal.data;
            size = nal.size;
            break;
        }
        if (nal.type == 5) { // IDR or SPS
            is_keyframe = true;
            data = nal.data;
            size = nal.size;
            break;
        }
    }
    if(data==nullptr || size==0){
        LOG_DEBUG("MP4", "没有可用帧");
        return false;
    }
//    LOG_DEBUGC("MP4", "data=%p, size=%d", data, size);

    // Convert Annex-B to AVCC
    std::vector<uint8_t> avcc_data = annexb_to_avcc(data, size);
    if (avcc_data.empty()) {
        std::cerr << "Failed to convert Annex-B to AVCC" << std::endl;
        return false;
    }

    // Create packet
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        std::cerr << "Failed to allocate packet" << std::endl;
        return false;
    }

    pkt->data = avcc_data.data();
    pkt->size = (int)avcc_data.size();
    pkt->stream_index = video_stream_->index;
    pkt->pts = pts_;
    pkt->dts = pts_;
    pkt->duration = 1;

    if (is_keyframe) {
        pkt->flags |= AV_PKT_FLAG_KEY;
    }

    // Rescale timestamps
    av_packet_rescale_ts(pkt, {1, 30}, video_stream_->time_base);

    int ret = av_interleaved_write_frame(fmt_ctx_, pkt);

    // Don't free pkt data since it points to our vector
    pkt->data = nullptr;
    pkt->size = 0;
    av_packet_free(&pkt);

    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        std::cerr << "Failed to write frame: " << err_buf << std::endl;
        return false;
    }

    pts_++;
    frame_count_++;

    if (frame_count_ % 30 == 0) {
        std::cout << "Written " << frame_count_ << " frames" << std::endl;
    }

    return true;
}

std::vector<MP4_Muxer::NalUnit> MP4_Muxer::parse_nal_units_optimized(const uint8_t* data, size_t size) {
    std::vector<NalUnit> nals;
    size_t i = 0;

    while (i < size) {
        // 1. 寻找起始码
        int sc_len = 0;
        if (i + 3 < size && data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x00 && data[i+3] == 0x01) {
            sc_len = 4;
        } else if (i + 2 < size && data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x01) {
            sc_len = 3;
        } else {
            i++;
            continue;
        }

        size_t nal_start = i + sc_len;
        if (nal_start >= size) break;

        uint8_t type = data[nal_start]; // 获取 NAL Header
        // H.264 类型提取通常为 data[nal_start] & 0x1F
        // 但根据你的描述，直接判断 0x41, 0x65 等特征值更高效

        // 2. 寻找当前 NALU 的终点
        size_t j = nal_start + 1;
        bool is_final_nal = (type == 0x41 || type == 0x65);

        if (is_final_nal) {
            // 如果是 P 帧或 IDR 帧，根据硬件特性，后面没有了，直接包含剩下所有数据
            nals.push_back({data + nal_start, size - nal_start, (uint8_t)(type & 0x1F)});
            break; // 核心优化：直接退出循环，不再遍历后续成百上千的字节
        }

        // 如果不是终点包（如 SPS/PPS/SEI），则需要寻找下一个起始码
        while (j < size) {
            if (j + 2 < size && data[j] == 0x00 && data[j+1] == 0x00 && data[j+2] == 0x01) {
                // 判定起始码长度，确定前一个 NALU 的边界
                size_t nal_end = (j > 0 && data[j-1] == 0x00) ? (j - 1) : j;
                nals.push_back({data + nal_start, nal_end - nal_start, (uint8_t)(type & 0x1F)});
                break;
            }
            j++;
        }

        if (j >= size) {
            // 兜底逻辑：处理最后一个包
            nals.push_back({data + nal_start, size - nal_start, (uint8_t)(type & 0x1F)});
        }

        i = j; // 移动到下一个 NALU 可能出现的位置
    }

    return nals;
}
std::vector<MP4_Muxer::NalUnit> MP4_Muxer::parse_nal_units(const uint8_t* data, size_t size) {
    std::vector<NalUnit> nals;
    size_t i = 0;

    while (i < size) {
        // Find start code
        int sc_len = 0;
        if (i + 3 < size && data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x00 && data[i+3] == 0x01) {
            sc_len = 4;
        } else if (i + 2 < size && data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x01) {
            sc_len = 3;
        } else {
            i++;
            continue;
        }

        size_t nal_start = i + sc_len;
        if (nal_start >= size) break;

        // Find end of this NAL unit
        size_t j = nal_start + 1;
        while (j < size) {
            if (j + 2 < size && data[j] == 0x00 && data[j+1] == 0x00 && data[j+2] == 0x01) {
                // Check for 4-byte start code
                if (j > 0 && data[j-1] == 0x00) {
                    nals.push_back({data + nal_start, j - 1 - nal_start, data[nal_start] & 0x1F});
                } else {
                    nals.push_back({data + nal_start, j - nal_start, data[nal_start] & 0x1F});
                }
                break;
            }
            j++;
        }

        if (j >= size) {
            // Last NAL unit
            nals.push_back({data + nal_start, size - nal_start, data[nal_start] & 0x1F});
        }

        i = nal_start;
    }

    return nals;
}
