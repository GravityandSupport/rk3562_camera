#include "videoplay.h"
#include <fstream>

VideoPlay::VideoPlay()
{

}

void VideoPlay::create(int width, int height, uint32_t buffer_num){
    m_width = width, m_height = height;
    buffer_num_=buffer_num;

    mjpeg_decoder.create(1920, 1088, 1);

    pool_buffer.initialize([this](DrmDumbBuffer& buf){
        buf.create(m_width, m_height, 12);
    });

    mjpeg_decoder.add_video(this);

    h264_decoder.create(1920, 1080, 2); // 缓冲区数量最好大一些，因为opengl可能和编码器会冲突
    h264_decoder.add_video(this);
    mp4_demuxer.add_video(&h264_decoder);
}
void VideoPlay::process_frames(VideoDrmBufPtr frame){
    LOG_DEBUG("DEBUG");
    DrmDumbBuffer* src_drm = frame->buffer;
    DrmDumbBuffer* dst_drm;

    std::lock_guard<std::mutex> lock(mutex_);

    ISlot* slot = pool_buffer.try_acquire();
    if(slot==nullptr) { LOG_DEBUG("VideoPlay", "无帧可用"); return ;}

    dst_drm = static_cast<DrmDumbBuffer*>(slot->getdata());
    if(dst_drm==nullptr) {return ;}

    RgaControl::resize(src_drm, dst_drm, RgaControl::Format::NV12, RgaControl::Format::NV12);

    {
        VideoDrmBufPtr frame = std::make_shared<VideoDrmBuf>();
        frame->video = this;
        frame->slot = slot;
        slot->retain();
        frames_ready(frame);
        slot->release();
    }
}
bool VideoPlay::decode_mp4(const std::string& filename){
    if(mp4_demuxer.open(filename)){
        mp4_demuxer.dump_info();
        return true;
    }
    return false;
}
bool VideoPlay::decode_jpeg(const std::string& filename)
{
    // 1. 以二进制方式打开文件
    std::ifstream file(filename.c_str(), std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "open file failed: " << filename << std::endl;
        return false;
    }

    // 2. 移动到文件末尾获取大小
    file.seekg(0, std::ios::end);
    std::streampos file_size = file.tellg();

    if (file_size <= 0) {
        std::cerr << "invalid file size" << std::endl;
        return false;
    }

    // 3. 回到开头
    file.seekg(0, std::ios::beg);

    // 4. 分配 buffer
    std::vector<uint8_t> buffer(static_cast<size_t>(file_size));

    // 5. 读取文件内容
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);

    if (!file) {
        std::cerr << "read file failed" << std::endl;
        return false;
    }

    // 6. 调用你的解码接口
    return mjpeg_decoder.decode_frame(buffer.data(), buffer.size());
}
