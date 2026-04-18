#include "videobase.h"

VideoBase::VideoBase(QObject *parent)
    : QObject(parent)
{

}

// ================== 数据分发 ==================

void VideoBase::frames_ready(VideoBase* capture, int idx){
    std::list<VideoNode> copy_list;

    {
        std::lock_guard<std::mutex> lock(mtx_);
        copy_list = video_list;   // ⭐ 关键：拷贝一份
    }

    for (auto& element : copy_list){
        if(element.enable_array_[static_cast<int>(ChannelType::NV12_INDEX)]){
            element.node->process_frames(capture, idx);
        }
    }
}

void VideoBase::frames_ready(VideoFramePtr frame) {
    std::list<VideoNode> copy_list;

    {
        std::lock_guard<std::mutex> lock(mtx_);
        copy_list = video_list;
    }

    for (auto& element : copy_list) {
        if(element.enable_array_[static_cast<int>(ChannelType::FRAME_DATA)]){
            element.node->process_frames(frame);
        }
    }
}

void VideoBase::frames_ready(VideoDrmBufPtr frame){
    std::list<VideoNode> copy_list;

    {
        std::lock_guard<std::mutex> lock(mtx_);
        copy_list = video_list;
    }

    for (auto& element : copy_list) {
        if(element.enable_array_[static_cast<int>(ChannelType::FRAME_DRMBUF)]){
            element.node->process_frames(frame);
        }
    }
}

// ================== 添加节点 ==================

void VideoBase::add_video(VideoBase* _video){
    if (!_video) return;

    VideoNode node;
    node.node = _video;
    node.enable_array_.fill(true);
    std::lock_guard<std::mutex> lock(mtx_);
    video_list.push_back(std::move(node));
}
void VideoBase::remove_video(VideoBase* video){
    if (!video) return;

    std::lock_guard<std::mutex> lock(mtx_);

    for (auto it = video_list.begin(); it != video_list.end(); ){
        if (it->node == video){
            it = video_list.erase(it);  // ⭐ erase返回下一个迭代器
        } else {
            ++it;
        }
    }
}
void VideoBase::set_enable(VideoBase* video, ChannelType channel, bool enable){
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto& element : video_list){
        if(element.node == video){
            element.enable_array_[static_cast<int>(channel)] = enable;
            return;
        }
    }
}
