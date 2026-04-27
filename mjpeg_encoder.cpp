#include "mjpeg_encoder.h"
#include "outLog.h"

#define MPP_ALIGN(x, a)         (((x)+(a)-1)&~((a)-1))

MJPEG_Encoder::MJPEG_Encoder(){
    // 创建MPP上下文
    MPP_RET ret = mpp_create(&ctx, &mpi);
    if (ret != MPP_OK) {
        printf("mpp_create failed, ret=%d\n", ret);
        return;
    }

    MppPollType timeout = MPP_POLL_BLOCK;
    ret = mpi->control(ctx, MPP_SET_OUTPUT_TIMEOUT, &timeout);
    if (MPP_OK != ret) {
        printf("mpi control set output timeout %d ret %d\n", timeout, ret);
        return;
    }

    // 初始化解码器
    ret = mpp_init(ctx, MPP_CTX_ENC, MPP_VIDEO_CodingMJPEG);
    if (ret != MPP_OK) {
        printf("mpp_init failed, ret=%d\n", ret);
        return;
    }



    printf("MPP JPEG Encoder init OK\n");
}

MJPEG_Encoder::~MJPEG_Encoder()
{
    if(group){
        mpp_buffer_group_put(group);
    }

    if(cfg){
        mpp_enc_cfg_deinit(cfg);
    }

    if (mpi && ctx) {
        mpi->reset(ctx);
    }

    if (ctx) {
        mpp_destroy(ctx);
        ctx = nullptr;
        mpi = nullptr;
    }
}

void MJPEG_Encoder::create(int width, int height, uint32_t buffer_num){
    m_width = width, m_height = height;
    buffer_num_=buffer_num;

    MPP_RET ret = MPP_OK;

    // 创建编码配置 cfg
    ret = mpp_enc_cfg_init(&cfg);
    if (ret != MPP_OK) {
        printf("mpp_enc_cfg_init failed, ret=%d\n", ret);
        return;
    }

    // 获取默认配置
    ret = mpi->control(ctx, MPP_ENC_GET_CFG, cfg);
    if (ret != MPP_OK) {
        printf("MPP_ENC_GET_CFG failed, ret=%d\n", ret);
        return;
    }

    // 修改编码配置
    mpp_enc_cfg_set_s32(cfg, "prep:width",  m_width);
    mpp_enc_cfg_set_s32(cfg, "prep:height", m_height);
    mpp_enc_cfg_set_s32(cfg, "prep:hor_stride", m_width);
    mpp_enc_cfg_set_s32(cfg, "prep:ver_stride", m_height);

    mpp_enc_cfg_set_u32(cfg, "jpeg:quant", 5);          // 量化参数 越小越清晰
    mpp_enc_cfg_set_s32(cfg, "jpeg:q_factor", 80); //可以采用默认值

    mpp_enc_cfg_set_s32(cfg, "codec:type", MPP_VIDEO_CodingMJPEG);

    // 应用配置
    ret = mpi->control(ctx, MPP_ENC_SET_CFG, cfg);
    if (ret != MPP_OK) {
        printf("MPP_ENC_SET_CFG failed, ret=%d\n", ret);
        return;
    }

    return;
}

static int frame_index = 0;
static bool saveJPEGToFile(const void* data, size_t size, const std::string& filename){
    FILE *fp = fopen(filename.c_str(), "wb");
    if(fp){
        fwrite(data, 1, size, fp);
        fclose(fp);
        return true;
    }
    return false;
}

bool MJPEG_Encoder::encode_frame(DrmDumbBuffer* drm_buf){
    MPP_RET ret = MPP_OK;

    // input / output
    MppPacket packet    = NULL;
    MppFrame  frame     = NULL;

    MppBuffer pkt_buf   = NULL;
    MppBuffer frm_buf   = NULL;

    MppBufferInfo info;
    memset(&info, 0, sizeof(info));
    info.type = MPP_BUFFER_TYPE_DRM;   // RK 平台正确类型
    info.fd   = drm_buf->get_dmabuf_fd();
    info.size = drm_buf->bytesused();

    ret = mpp_buffer_import(&frm_buf, &info);
    if (ret) {
        printf("mpp_buffer_import failed %d\n", ret);
        goto ENCODE_OUT;
    }

    ret = mpp_frame_init(&frame);
    if (ret) {
        printf("mpp_frame_init failed\n");
        goto ENCODE_OUT;
    }

    mpp_frame_set_width(frame, drm_buf->width());
    mpp_frame_set_height(frame, drm_buf->height());
    mpp_frame_set_hor_stride(frame, MPP_ALIGN(drm_buf->width(), 16));
    mpp_frame_set_ver_stride(frame, MPP_ALIGN(drm_buf->height(), 16));
    mpp_frame_set_fmt(frame, MPP_FMT_YUV420SP);

    mpp_frame_set_buffer(frame, frm_buf);

    ret = mpi->encode_put_frame(ctx, frame);
    if (ret) {
        printf("encode put frame failed\n");
        goto ENCODE_OUT;
    }

    ret = mpi->encode_get_packet(ctx, &packet);
    if (ret) {
        printf("encode get packet failed\n");
        goto ENCODE_OUT;
    }

    if (packet) {
        uint8_t * data = (uint8_t *)mpp_packet_get_data(packet);
        size_t size = mpp_packet_get_length(packet);
        VideoFramePtr frame = std::make_shared<VideoFrame>();
        frame->width = m_width;
        frame->height = m_height;
        frame->stride = m_width;
        frame->data = std::make_shared<std::vector<uint8_t>>(data, data+size);
//        LOG_DEBUG("h264", size);
        frames_ready(frame);
        saveJPEGToFile(data, size, std::string("/mnt/nfs_dir/mjpeg")+std::to_string(frame_index)+std::string(".jpg"));
    }
//    LOG_DEBUG("ENCODE", frm_buf, pkt_buf, frame, packet);

ENCODE_OUT:
    if(frm_buf){
        mpp_buffer_put(frm_buf);
    }

    if(pkt_buf){
        mpp_buffer_put(pkt_buf);
    }

    if (frame) {
        mpp_frame_deinit(&frame);
    }

    if (packet){
        mpp_packet_deinit(&packet);
    }

    return (ret==MPP_OK);
}

void MJPEG_Encoder::process_frames(VideoDrmBufPtr frame){
    ISlot* slot = frame->slot;
    if(slot==nullptr) {return ;}
    slot->retain();
//    LOG_DEBUG("mjpeg encode");
    if(frame_index>100 && frame_index<106){
        encode_frame(static_cast<DrmDumbBuffer*>(slot->getdata()));
    }
    frame_index++;
    slot->release();
}





