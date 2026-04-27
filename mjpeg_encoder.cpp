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

    drm_buf.resize(buffer_num_);
    for(auto& buf : drm_buf){
        buf = std::make_shared<DrmDumbBuffer>();
        buf->create(m_width, m_height, 8);
    }

    // 按照官方文档描述，对于H.264/H.265 需要提供20+缓冲区，其他编码需要提供10+缓冲区，一开始就是因为提供的缓冲区比较少，所以导致申请不到mpp_buffer，后面我改到10个缓冲区后就正常了
    ret = mpp_buffer_group_get_external(&group, MPP_BUFFER_TYPE_DRM);
    if (ret) {
        printf("failed to get mpp buffer group ret %d\n", ret);
        goto MPP_OUT;
    }

    MppBufferInfo commit;
    commit.type = MPP_BUFFER_TYPE_DRM;
    commit.size = drm_buf[0]->size();
    for (size_t i = 0; i < drm_buf.size(); i++){
        commit.index = i;
        commit.ptr = drm_buf[i]->map();
        commit.fd = drm_buf[i]->get_dmabuf_fd();

        ret = mpp_buffer_commit(group, &commit);
        if (ret) {
            printf("external buffer commit failed ret %d\n", ret);
            break;
        }
    }

MPP_OUT:
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

    ret = mpp_buffer_get(group, &pkt_buf, m_width*m_height);
    if (ret) {
        printf("failed to get buffer for output packet ret %d\n", ret);
        goto ENCODE_OUT;
    }

    mpp_packet_init_with_buffer(&packet, pkt_buf);
    mpp_packet_set_length(packet, 0);

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
//        MppBuffer buffer = mpp_packet_get_buffer(packet);
        void *ptr   = mpp_packet_get_pos(packet);
        size_t len  = mpp_packet_get_length(packet);
//        int index = mpp_buffer_get_index(buffer);
        LOG_DEBUG("encoder", ptr, len/*, index*/);
        saveJPEGToFile(ptr, len, std::string("/mnt/nfs_dir/mjpeg")+std::to_string(frame_index)+std::string(".jpg"));
    }
    LOG_DEBUG("ENCODE", frm_buf, pkt_buf, frame, packet);

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
    if(frame_index>10 && frame_index<20){
        encode_frame(static_cast<DrmDumbBuffer*>(slot->getdata()));
    }
    frame_index++;
    slot->release();
}





