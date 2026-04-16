#include "h264_encoder.h"
#include "v4l2_nv12_capture.h"
#include "outLog.h"
#include <iostream>
#include <iomanip> // 必须包含，用于格式化输出

H264_Encoder::H264_Encoder():
    process_queue(3)
{

}

void H264_Encoder::process_frames(VideoBase* capture, int idx){
    NV12_Packet packet = {capture, idx};
    process_queue.push(std::move(packet));
}

bool H264_Encoder::initMPP()
{
    if (mpp_create(&m_mppCtx, &m_mppApi) != MPP_OK)
        return false;

    if (mpp_init(m_mppCtx, MPP_CTX_ENC, MPP_VIDEO_CodingAVC) != MPP_OK){
        std::cerr << "mpp_init fail\n";
        return false;
    }

    mpp_enc_cfg_init(&m_encCfg);
    m_mppApi->control(m_mppCtx, MPP_ENC_GET_CFG, m_encCfg);

    mpp_enc_cfg_set_s32(m_encCfg, "codec:type", MPP_VIDEO_CodingAVC);

    /* setup preprocess parameters */
    mpp_enc_cfg_set_s32(m_encCfg, "prep:width", m_width);
    mpp_enc_cfg_set_s32(m_encCfg, "prep:height", m_height);
    mpp_enc_cfg_set_s32(m_encCfg, "prep:hor_stride", m_width);
    mpp_enc_cfg_set_s32(m_encCfg, "prep:ver_stride", m_height);
    mpp_enc_cfg_set_s32(m_encCfg, "prep:format", MPP_FMT_YUV420SP);
    mpp_enc_cfg_set_s32(m_encCfg, "prep:range", MPP_FRAME_RANGE_JPEG);

    int64_t bps_target = m_width*m_height*m_fps * 1.0f;
    std::clog << "bps_target=" << bps_target << std::endl;

    mpp_enc_cfg_set_s32(m_encCfg, "rc:mode", MPP_ENC_RC_MODE_CBR);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:bps_target", bps_target);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:fps_in_flex", 0);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:fps_in_num", m_fps);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:fps_in_denorm", 1);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:fps_out_num", m_fps);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:fps_out_denorm", 1);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:gop", m_fps * 2);

    mpp_enc_cfg_set_s32(m_encCfg, "h264:profile", 100);
    mpp_enc_cfg_set_s32(m_encCfg, "h264:level", 41);

    m_mppApi->control(m_mppCtx, MPP_ENC_SET_CFG, m_encCfg);

    // 4. 关键：设置header模式
    // 设置一下mpp编码头部信息，否则编码出来的 SPS/PPS 默认只存在于每一帧，设置这个后每个IDR关键帧都会带有SPS/PPS
    MppEncHeaderMode header_mode = MPP_ENC_HEADER_MODE_EACH_IDR;
    m_mppApi->control(m_mppCtx, MPP_ENC_SET_HEADER_MODE, &header_mode);
    return true;
}

bool H264_Encoder::encodeFrame(const DrmDumbBuffer* input){
    MppFrame frame = NULL;
    MppBuffer buffer = NULL;
    MppPacket packet = NULL;

    mpp_frame_init(&frame);
    MppBufferInfo info;
    memset(&info, 0, sizeof(info));
    info.type = MPP_BUFFER_TYPE_DRM;   // RK 平台正确类型
    info.fd   = input->get_dmabuf_fd();
    info.size = m_width * m_height * 3 / 2;
    mpp_buffer_import(&buffer, &info);

    mpp_frame_set_width(frame,  m_width);
    mpp_frame_set_height(frame, m_height);
    mpp_frame_set_hor_stride(frame, m_width);
    mpp_frame_set_ver_stride(frame, m_height);

    mpp_frame_set_fmt(frame, MPP_FMT_YUV420SP);

    mpp_frame_set_buffer(frame, buffer);

    m_mppApi->encode_put_frame(m_mppCtx, frame);

    if (m_mppApi->encode_get_packet(m_mppCtx, &packet) == MPP_OK){
        uint8_t * data = (uint8_t *)mpp_packet_get_data(packet);
        size_t size = mpp_packet_get_length(packet);
        VideoFramePtr frame = std::make_shared<VideoFrame>();
        frame->width = m_width;
        frame->height = m_height;
        frame->stride = m_width;
        frame->data = std::make_shared<std::vector<uint8_t>>(data, data+size);
        frames_ready(frame);

        mpp_packet_deinit(&packet);
    }

    mpp_frame_deinit(&frame);
    mpp_buffer_put(buffer);

    return true;
}


bool H264_Encoder::start_encoder(int width, int height, int fps){
    m_width=width, m_height=height, m_fps=fps;

    if (!initMPP()){
        return false;
    }

    thread_.set_loop_callback([this](SafeThread* self) ->bool{
        (void)self;

        NV12_Packet packet;
        if(process_queue.pop(packet)){
            if(packet.capture==nullptr) {return true;}
            V4L2_NV12_Capture* capture = static_cast<V4L2_NV12_Capture*>(packet.capture);

            DrmDumbBuffer* drm = capture->acquire_dmabuf(packet.idx);
            if(drm==nullptr) {return true;}

            encodeFrame(drm);

            capture->release_dmabuf(packet.idx);
        }

        return true;
    });
    thread_.start();

    return true;
}

bool H264_Encoder::stop_encoder(){
    m_mppApi->reset(m_mppCtx);
    mpp_destroy(m_mppCtx);

    process_queue.close();
    thread_.stop();
    return true;
}
