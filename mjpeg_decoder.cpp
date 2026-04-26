#include "mjpeg_decoder.h"
#include "outLog.h"

#define MPP_ALIGN(x, a)         (((x)+(a)-1)&~((a)-1))

void MJPEG_Decoder::create(int width, int height, uint32_t buffer_num){
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
}

MJPEG_Decoder::MJPEG_Decoder(){
    // 创建MPP上下文
    MPP_RET ret = mpp_create(&ctx, &mpi);
    if (ret != MPP_OK) {
        printf("mpp_create failed, ret=%d\n", ret);
        return;
    }

    // 初始化解码器
    ret = mpp_init(ctx, MPP_CTX_DEC, MPP_VIDEO_CodingMJPEG);
    if (ret != MPP_OK) {
        printf("mpp_init failed, ret=%d\n", ret);
        return;
    }

    // 设置编码器输出格式
    MppFrameFormat format = MPP_FMT_YUV420SP;
    ret = mpi->control(ctx, MPP_DEC_SET_OUTPUT_FORMAT, &format);
    if (ret != MPP_OK){
        printf("Failed to set output format 0x%x\n", format);
        return;
    }

    printf("MPP decoder initialized\n");
}

MJPEG_Decoder::~MJPEG_Decoder(){
    if(group){
        mpp_buffer_group_put(group);
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

bool MJPEG_Decoder::decode_frame(uint8_t* data, size_t length){
    MPP_RET ret = MPP_OK;

    // input / output
    MppPacket packet    = NULL;
    MppFrame  frame     = NULL;

    MppBuffer pkt_buf   = NULL;
    MppBuffer frm_buf   = NULL;

    MppTask task = NULL;

    if(!find_jpg_end_marker(data, length)){ // 没有包含jpg结束符
        printf("jpeg: %02x %02x ... %02x %02x %02x length=%ld\n",
        data[0], data[1],
        data[length-3], data[length-2], data[length-1],
        length);

        return false;
    }

    ret = mpp_frame_init(&frame);
    if (MPP_OK != ret) {
        printf("mpp_frame_init failed\n");
        goto DECODE_OUT;
    }

    ret = mpp_buffer_get(group, &frm_buf, m_width*m_height*2);
    if (MPP_OK != ret) {
        printf("failed to get buffer for input frame ret %d\n", ret);
        goto DECODE_OUT;
    }
    mpp_frame_set_buffer(frame, frm_buf);

    ret = mpp_buffer_get(NULL, &pkt_buf, MPP_ALIGN(length, 16));
    if (MPP_OK != ret) {
        printf("failed to get buffer for input frame ret %d\n", ret);
        goto DECODE_OUT;
    }
    memcpy((RK_U8*)mpp_buffer_get_ptr(pkt_buf), data, length);
    mpp_packet_init_with_buffer(&packet, pkt_buf);

    ret = mpi->poll(ctx, MPP_PORT_INPUT, MPP_POLL_BLOCK);
    if (ret) {
        printf("%p mpp input poll failed\n", ctx);
        goto DECODE_OUT;
    }

    ret = mpi->dequeue(ctx, MPP_PORT_INPUT, &task);  /* input queue */
    if (ret) {
        printf("%p mpp task input dequeue failed\n", ctx);
        goto DECODE_OUT;
    }

    // mpp_assert(task);

    mpp_task_meta_set_packet(task, KEY_INPUT_PACKET, packet);
    mpp_task_meta_set_frame (task, KEY_OUTPUT_FRAME,  frame);

    ret = mpi->enqueue(ctx, MPP_PORT_INPUT, task);  /* input queue */
    if (ret) {
        printf("%p mpp task input enqueue failed\n", ctx);
        goto DECODE_OUT;
    }

    /* poll and wait here */
    ret = mpi->poll(ctx, MPP_PORT_OUTPUT, MPP_POLL_BLOCK);
    if (ret) {
        printf("%p mpp output poll failed\n", ctx);
        goto DECODE_OUT;
    }

    ret = mpi->dequeue(ctx, MPP_PORT_OUTPUT, &task); /* output queue */
    if (ret) {
        printf("%p mpp task output dequeue failed\n", ctx);
        goto DECODE_OUT;
    }

    if (task){
        MppFrame frame_out = NULL;
        mpp_task_meta_get_frame(task, KEY_OUTPUT_FRAME, &frame_out);
        
        // ==================================
        if (frame){
            MppBuffer buffer = mpp_frame_get_buffer(frame_out);
            int index = mpp_buffer_get_index(buffer);
            // size_t size = mpp_buffer_get_size(buffer);
            // void *ptr = mpp_buffer_get_ptr(buffer);
            // printf("编码成功，buffer.size=%ld, index=%d\n", size, index);
//            int width  = mpp_frame_get_width(frame);
//            int height = mpp_frame_get_height(frame);
            int hor_stride = mpp_frame_get_hor_stride(frame);
            int ver_stride = mpp_frame_get_ver_stride(frame);
//            LOG_DEBUG("mjpeg", width, height, hor_stride, ver_stride);

            VideoDrmBufPtr drm_frame = std::make_shared<VideoDrmBuf>();
            drm_frame->video = this;
            drm_frame->buffer = drm_buf[index].get();
            drm_frame->buffer->setWidth(hor_stride); // mjpeg解码后，并不会使用使用全部的内存空间，所以图像的宽高和内存的宽高并不匹配
            drm_frame->buffer->setHeight(ver_stride);
            drm_frame->buffer->setBpp(12);
            frames_ready(drm_frame);

//            if(frame_index==10){
//                LOG_DEBUG("mjpeg", drm_frame->buffer->bytesused()/*, drm_frame->buffer->getSize()*/);
////                saveNV12ToFile(drm_buf[index]->map(), hor_stride*ver_stride*3/2, "/mnt/nfs_dir/mjpeg.yuv");
//            }
//            frame_index++;
        }
        // ----------------------------------

        /* output queue */
        ret = mpi->enqueue(ctx, MPP_PORT_OUTPUT, task);
        if (ret){
            printf("%p mpp task output enqueue failed\n", ctx);
            goto DECODE_OUT;
        }
    }

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

void MJPEG_Decoder::process_frames(VideoDrmBufPtr frame){
    if(frame->usb_buffer.start){
        // LOG_DEBUG("decoder");
        decode_frame(static_cast<uint8_t*>(frame->usb_buffer.start), frame->usb_buffer.length);
    }
}

bool MJPEG_Decoder::find_jpg_end_marker(const uint8_t* data, size_t length) {
    // 安全检查：由于从 length-5 开始搜索，且标志占 2 字节
    // 我们至少需要 length >= 5 来确保搜索起点有效，
    // 且数组至少有 2 字节来容纳 FF D9
    if (data == nullptr || length < 5) {
        return false;
    }

    // 从 length - 5 的位置开始遍历
    // 这里的 i <= length - 2 是因为 FF D9 需要占用两个连续位置
    for (size_t i = length - 5; i <= length - 2; ++i) {
        if (data[i] == 0xFF && data[i + 1] == 0xD9) {
            return true;
        }
    }

    return false;
}
