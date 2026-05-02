#include "h264_decoder.h"
#include "outLog.h"

H264_Decoder::H264_Decoder() : process_queue(5)
{
    // 创建MPP上下文
    MPP_RET ret = mpp_create(&m_mppCtx, &m_mppApi);
    if (ret != MPP_OK) {
        printf("mpp_create failed, ret=%d\n", ret);
        return ;
    }

    // 初始化解码器
    ret = mpp_init(m_mppCtx, MPP_CTX_DEC, MPP_VIDEO_CodingAVC);
    if (ret != MPP_OK) {
        printf("mpp_init failed, ret=%d\n", ret);
        return ;
    }
}

H264_Decoder::~H264_Decoder(){
    if(group){
        mpp_buffer_group_put(group);
    }

    if (m_mppApi && m_mppCtx) {
        m_mppApi->reset(m_mppCtx);
    }

    if (m_mppCtx) {
        mpp_destroy(m_mppCtx);
        m_mppCtx = nullptr;
        m_mppApi = nullptr;
    }
}

void H264_Decoder::create(int width, int height, uint32_t buffer_num){
    m_width = width, m_height = height;
    buffer_num_=buffer_num;

    drm_buf.resize(buffer_num_);
    for(auto& buf : drm_buf){
        buf = std::make_shared<DrmDumbBuffer>();
        buf->create(m_width, m_height, 16);
    }

    MPP_RET ret = mpp_buffer_group_get_external(&group, MPP_BUFFER_TYPE_DRM);
    if (ret) {
        printf("get mpp external buffer group failed ret %d\n", ret);
        return;
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

    thread_.set_loop_callback([this](SafeThread* self) ->bool{
        (void)self;

        VideoFramePtr frame;
        if(process_queue.pop(frame)){
            decode_frame(frame->data->data(), frame->data->size());
        }

        return true;
    });
    thread_.start();
}


#include <fstream>
static bool saveNV12ToFile(const void* data, size_t size, const std::string& filename) {
    if (!data || size == 0) {
        std::cerr << "Error: Invalid data pointer or size." << std::endl;
        return false;
    }

    // 使用二进制模式打开文件
    std::ofstream outfile(filename, std::ios::out | std::ios::binary);

    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << filename << std::endl;
        return false;
    }

    // 写入数据
    outfile.write(reinterpret_cast<const char*>(data), size);

    if (outfile.good()) {
        std::cout << "Successfully saved " << size << " bytes to " << filename << std::endl;
        return true;
    } else {
        std::cerr << "Error: Occurred during writing to " << filename << std::endl;
        return false;
    }
}
static int frame_index = 0;

bool H264_Decoder::decode_frame(uint8_t* data, size_t length){
    MPP_RET ret = MPP_OK;

    // input / output
    MppPacket packet    = NULL;
    MppFrame  frame     = NULL;

    MppBuffer pkt_buf   = NULL;
    MppBuffer frm_buf   = NULL;

    RK_S32 times = 30;

    ret = mpp_packet_init(&packet, NULL, 0);
    if (ret) {
        printf("mpp_packet_init failed\n");
        goto DECODE_OUT;
    }

    mpp_packet_set_data(packet, data);
    mpp_packet_set_size(packet, length);
    mpp_packet_set_pos(packet, data);
    mpp_packet_set_length(packet, length);

    ret = m_mppApi->decode_put_packet(m_mppCtx, packet);
    if (MPP_OK != ret){
        printf("decode_put_packet failed, ret=%d\n", ret);
        goto DECODE_OUT;
    }

    do{
        ret = m_mppApi->decode_get_frame(m_mppCtx, &frame);
        if (MPP_ERR_TIMEOUT == ret) {
            if (times > 0) {
                times--;
                SafeThread::msDelay(1);
                continue;
            }
            printf("%p decode_get_frame failed too much time\n", m_mppCtx);
        }

        if (ret) {
            printf("%p decode_get_frame failed ret %d\n", m_mppCtx, ret);
            break;
        }

        if (frame) {
            if (mpp_frame_get_info_change(frame)){
                RK_U32 width = mpp_frame_get_width(frame);
                RK_U32 height = mpp_frame_get_height(frame);
                RK_U32 hor_stride = mpp_frame_get_hor_stride(frame);
                RK_U32 ver_stride = mpp_frame_get_ver_stride(frame);
                RK_U32 buf_size = mpp_frame_get_buf_size(frame);
                LOG_DEBUG("h264 decoder", width, height, hor_stride, ver_stride, buf_size);

                mpp_buffer_group_clear(group);
                drm_buf.clear();

                drm_buf.resize(buffer_num_);
                for(auto& buf : drm_buf){
                    buf = std::make_shared<DrmDumbBuffer>();
                    buf->create(hor_stride, ver_stride, 16);
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

                ret = m_mppApi->control(m_mppCtx, MPP_DEC_SET_EXT_BUF_GROUP, group);
                if (ret) {
                    printf("%p set buffer group failed ret %d\n", m_mppCtx, ret);
                    break;
                }

                ret = m_mppApi->control(m_mppCtx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
                if (ret) {
                    printf("%p info change ready failed ret %d\n", m_mppCtx, ret);
                    break;
                }
            }else{
                MppBuffer buffer = mpp_frame_get_buffer(frame);
                RK_U32 width = mpp_frame_get_width(frame);
                RK_U32 height = mpp_frame_get_height(frame);
                RK_U32 hor_stride = mpp_frame_get_hor_stride(frame);
                RK_U32 ver_stride = mpp_frame_get_ver_stride(frame);
                RK_U32 buf_size = mpp_frame_get_buf_size(frame);
                int index = mpp_buffer_get_index(buffer);
//                LOG_DEBUG("h264 decoder", width, height, hor_stride, ver_stride, buf_size, index);
                drm_buf[index]->bytesused(hor_stride*ver_stride*3/2);

//                if(frame_index%100==0 && frame_index<1000){
//                    std::ostringstream oss;
//                    oss << "/mnt/nfs_dir/h264_decode-" << frame_index << ".yuv";
//                    saveNV12ToFile(drm_buf[index]->map(), drm_buf[index]->bytesused(), oss.str());
//                }
//                frame_index++;
            }
            break;
        }
    }while(1);

DECODE_OUT:
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

void H264_Decoder::process_frames(VideoFramePtr frame){
//    decode_frame(frame->data->data(), frame->data->size());
    process_queue.push(frame);
}
