#include "ImageDisplay.h"
#include "outLog.h"

ImageDisplay::ImageDisplay(QObject *parent) : QObject(parent), process_queue(5)
{

}

ImageDisplay::~ImageDisplay(){}

void ImageDisplay::create(DmaBufRenderer* _render){
    render = _render;
    if(render){
        connect(this, &ImageDisplay::onNewDmaBuf_signals, render, &DmaBufRenderer::onNewDmaBuf_slots);
    }

    thread_.set_loop_callback([this](SafeThread* self) ->bool{
        (void)self;

        VideoDrmBufPtr frame;
        if(process_queue.pop(frame)){
            if(render){
                render->show_queue.push(frame);
                emit onNewDmaBuf_signals();
            }
        }

        return true;
    });
    thread_.start("h264 encoder");
}

void ImageDisplay::process_frames(VideoDrmBufPtr frame){
    ISlot* slot = frame->slot;
    if(slot==nullptr) {return ;}

    slot->retain();
    process_queue.push(frame);
}
