#include "photosave.h"
#include "outLog.h"
#include "mjpeg_encoder.h"

PhotoSave::PhotoSave() : process_queue(3), file_queue(40)
{

}
bool PhotoSave::saveJPEGToFile(const void* data, size_t size, const std::string& filename){
    FILE *fp = fopen(filename.c_str(), "wb");
    if(fp){
        fwrite(data, 1, size, fp);
        fclose(fp);
        return true;
    }
    return false;
}
void PhotoSave::create(MJPEG_Encoder* input_source){
    input_source_ = input_source;
}
void PhotoSave::process_frames(VideoFramePtr frame){
    process_queue.push(frame);
    LOG_DEBUG("debug");
}
bool PhotoSave::save(const std::string name){
    bool ret = false;
    if(input_source_){input_source_->node_state.enable();}
    VideoFramePtr frame;
    if(process_queue.timed_pop(frame, 1000)){
        ret = saveJPEGToFile(frame->data->data(), frame->data->size(), name);
    }else{
        LOG_DEBUG("PhotoSave", "拍照失败，因为没有流");
    }
    if(input_source_){input_source_->node_state.disable();}
    return ret;
}
