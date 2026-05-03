#include "videomerge.h"
#include "rgacontrol.h"
#include "outLog.h"
#include "v4l2_nv12_capture.h"

VideoMerge::Node VideoMerge::set(Node& node, const VideoBase* v, uint32_t nx, uint32_t ny, uint32_t nw, uint32_t nh){
    std::lock_guard<std::mutex> lock(mutex_);

    Node old_state = node;
    old_state.drm_buffer_.reset();

    node.video = v;
    node.drm_buffer_ = std::make_shared<DrmDumbBuffer>();
    node.x = node.rect.x = nx;
    node.y = node.rect.y = ny;
    node.width_ = node.rect.width = nw;
    node.height_ = node.rect.height = nh;
    node.drm_buffer_->create(node.width_, node.height_, 12);
    return old_state;
}
const VideoBase* VideoMerge::set(Node& node, const VideoBase* v){
    std::lock_guard<std::mutex> lock(mutex_);
    const VideoBase* ret = node.video;
    node.video = v;
    return ret;
}
VideoMerge::Node VideoMerge::setBigNode(const VideoBase* v, uint32_t nx, uint32_t ny, uint32_t nw, uint32_t nh){
    return set(big_source, v, nx, ny, nw, nh);
}
VideoMerge::Node VideoMerge::setSmallNode(const VideoBase* v, uint32_t nx, uint32_t ny, uint32_t nw, uint32_t nh){
    return set(small_source, v, nx, ny, nw, nh);
}
const VideoBase* VideoMerge::setBigNode(const VideoBase* v){
    return set(big_source, v);
}
const VideoBase* VideoMerge::setSmallNode(const VideoBase* v){
    return set(small_source, v);
}
void VideoMerge::create(uint32_t width, uint32_t height,
            const std::string& drm_dev){
    width_ = width;
    height_ = height;
    drm_dev_ = drm_dev;

    pool_buffer.initialize([this](DrmDumbBuffer& buf){
        buf.create(width_, height_, 12);
    });

    thread_.set_loop_callback([this](SafeThread* self) ->bool{
        (void)self;
        VideoDrmBufPtr frame;
        if(process_queue.pop(frame)){
            ISlot* slot = frame->slot;
            if(slot==nullptr) {return true;}
            slot->retain();
            frames_ready(frame);
            slot->release();
        }

        return true;
    });
    thread_.start("video merge");
}

void VideoMerge::process_frames(VideoDrmBufPtr frame){
    DrmDumbBuffer* src_drm = frame->buffer;
    DrmDumbBuffer* dst_drm;

    std::lock_guard<std::mutex> lock(mutex_);

    Node* source = nullptr;
    if(big_source.video==frame->video){
        source = &big_source;
    }else if(small_source.video==frame->video){
        source = &small_source;
    }else{return;}

    dst_drm = source->drm_buffer_.get();

    RgaControl::resize(src_drm, dst_drm, RgaControl::Format::NV12, RgaControl::Format::NV12);
    source->status = true;

//    LOG_DEBUG("video merge", pool_buffer.free_count(), big_source.status, small_source.status, src_drm->size(), dst_drm->size());
    if(big_source.status && small_source.status){
        big_source.status=false;
        small_source.status=false;

        ISlot* slot = pool_buffer.try_acquire();
        if(slot==nullptr) { LOG_DEBUG("video merge", "无帧可用"); return ;}

        DrmDumbBuffer* drm_buffer = static_cast<DrmDumbBuffer*>(slot->getdata());
        if(drm_buffer==nullptr) {return ;}

        RgaControl::resize_rect(big_source.drm_buffer_.get(), drm_buffer, RgaControl::Format::NV12, RgaControl::Format::NV12, big_source.rect);
        RgaControl::resize_rect(small_source.drm_buffer_.get(), drm_buffer, RgaControl::Format::NV12, RgaControl::Format::NV12, small_source.rect);
        VideoDrmBufPtr frame = std::make_shared<VideoDrmBuf>();
        frame->video = this;
        frame->slot = slot;
        process_queue.push(frame);
    }
}

