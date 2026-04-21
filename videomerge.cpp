#include "videomerge.h"
#include "rgacontrol.h"
#include "outLog.h"
#include "v4l2_nv12_capture.h"

void VideoMerge::create(uint32_t width, uint32_t height,
            const std::string& drm_dev){
    width_ = width;
    height_ = height;
    drm_dev_ = drm_dev;

    big_source.drm_buffer_.create(big_source.width_, big_source.height_, 12);
    big_source.rect.x = big_source.x;
    big_source.rect.y = big_source.y;
    big_source.rect.width = big_source.width_;
    big_source.rect.height = big_source.height_;
    small_source.drm_buffer_.create(small_source.width_, small_source.height_, 12);
    small_source.rect.x = small_source.x;
    small_source.rect.y = small_source.y;
    small_source.rect.width = small_source.width_;
    small_source.rect.height = small_source.height_;

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

    dst_drm = &source->drm_buffer_;

    RgaControl::resize(src_drm, dst_drm, RgaControl::Format::NV12, RgaControl::Format::NV12);
    source->status = true;

//    LOG_DEBUG("video merge", pool_buffer.free_count(), big_source.status, small_source.status, src_drm->size(), dst_drm->size());
    if(big_source.status && small_source.status){
        big_source.status=false;
        small_source.status=false;

        ISlot* slot = pool_buffer.try_acquire();
        if(slot==nullptr) {return ;}

        DrmDumbBuffer* drm_buffer = static_cast<DrmDumbBuffer*>(slot->getdata());
        if(drm_buffer==nullptr) {return ;}

        RgaControl::resize_rect(&big_source.drm_buffer_, drm_buffer, RgaControl::Format::NV12, RgaControl::Format::NV12, big_source.rect);
        RgaControl::resize_rect(&small_source.drm_buffer_, drm_buffer, RgaControl::Format::NV12, RgaControl::Format::NV12, small_source.rect);
        VideoDrmBufPtr frame = std::make_shared<VideoDrmBuf>();
        frame->video = this;
        frame->slot = slot;
        process_queue.push(frame);
    }
}

