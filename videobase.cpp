#include "videobase.h"

VideoBase::VideoBase(QObject *parent)
    : QObject(parent)
{
    channel_array_.fill(true);  // 默认全部开启
}

// ================== 通道控制 ==================

void VideoBase::enable_channel(ChannelType type, bool enable){
    std::lock_guard<std::mutex> lock(mtx_);
    channel_array_[static_cast<int>(type)] = enable;
}

bool VideoBase::is_channel_enabled(ChannelType type){
    std::lock_guard<std::mutex> lock(mtx_);
    return channel_array_[static_cast<int>(type)];
}
bool VideoBase::is_channel_enabled_nolock(ChannelType type){
    return channel_array_[static_cast<int>(type)];
}

// ================== 数据分发 ==================

void VideoBase::frames_ready(VideoBase* capture, int idx){
    std::list<VideoBase*> copy_list;

    {
        std::lock_guard<std::mutex> lock(mtx_);
        copy_list = video_list;   // ⭐ 关键：拷贝一份
    }

    for (auto& element : copy_list){
        if(element->is_channel_enabled_nolock(ChannelType::NV12_INDEX)){
            element->process_frames(capture, idx);
        }
    }
}

void VideoBase::frames_ready(VideoFramePtr frame) {
    std::list<VideoBase*> copy_list;

    {
        std::lock_guard<std::mutex> lock(mtx_);
        copy_list = video_list;
    }

    for (auto& element : copy_list) {
        if(element->is_channel_enabled_nolock(ChannelType::FRAME_DATA)){
            element->process_frames(frame);
        }
    }
}

// ================== 添加节点 ==================

void VideoBase::add_video(VideoBase* _video){
    std::lock_guard<std::mutex> lock(mtx_);
    video_list.push_back(_video);
}
