#include "videobase.h"

VideoBase::VideoBase(QObject *parent) : QObject(parent)
{

}

void VideoBase::frames_ready(VideoBase* capture, int idx){
    for (auto& element : video_list){
        element->process_frames(capture, idx);
    }
}

void VideoBase::frames_ready(VideoFramePtr frame) {
    for (auto& element : video_list) {
        element->process_frames(frame);
    }
}

void VideoBase::add_video(VideoBase* _video){
    video_list.push_back(_video);
}
