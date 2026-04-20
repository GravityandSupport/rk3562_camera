#include "videomerge.h"
#include "rgacontrol.h"
#include "outLog.h"
#include "v4l2_nv12_capture.h"

void VideoMerge::create(uint32_t width, uint32_t height,
            const std::string& drm_dev){
    width_ = width;
    height_ = height;
    drm_dev_ = drm_dev;

    for (std::size_t i = 0; i < ring_buffer.capacity(); ++i){
        *ring_buffer.at(i) = std::make_shared<DrmDumbBuffer>();
        if(!ring_buffer.at(i)->get()->create(drm_dev_.c_str(), width_, height_, 12)){
            LOG_DEBUG("video merge", i);
            ring_buffer.at(i)->reset();
        }
    }
}

void VideoMerge::process_frames(VideoBase* _capture, int idx){
    V4L2_NV12_Capture* capture = static_cast<V4L2_NV12_Capture*>(_capture);
    DrmDumbBuffer* src_drm = capture->acquire_dmabuf(idx);
    if(src_drm==nullptr) {return;}

//    if(big_source.video==nullptr || small_source.video==nullptr) {return;}

    DrmDumbBuffer* dst_drm = ring_buffer.current()->get();
    im_rect rect;
//    rect.x = 0;
//    rect.y = 0;
//    rect.width = dst_drm->width();
//    rect.height = dst_drm->height();
    if(big_source.video==capture){
        rect.x = 0;
        rect.y = 0;
        rect.width = dst_drm->width();
        rect.height = dst_drm->height();
        big_source.status = true;
    }else if(small_source.video==capture){
        rect.x = 0;
        rect.y = 0;
        rect.width = 208;
        rect.height = 128;
        small_source.status = true;
    }else{return;}

    RgaControl::resize_rect(src_drm, dst_drm, RgaControl::Format::NV12, RgaControl::Format::NV12, rect);

//    VideoDrmBufPtr frame = std::make_shared<VideoDrmBuf>();
//    frame->video = this;
//    frame->buffer = dst_drm;
//    frames_ready(frame);

    LOG_DEBUG("video merge", idx, big_source.status, small_source.status, src_drm->size(), dst_drm->size());
    if(big_source.status && small_source.status){
        VideoDrmBufPtr frame = std::make_shared<VideoDrmBuf>();
        frame->video = this;
        frame->buffer = dst_drm;
        frames_ready(frame);

        big_source.status=false;
        small_source.status=false;
        ring_buffer.next();
    }

//    capture->release_dmabuf(idx);
}
