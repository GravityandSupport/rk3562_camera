#include "h264_nalusave.h"
#include <fstream>
#include <iomanip>
#include "outLog.h"
#include "h264_encoder.h"
#include "JsonWrapper.h"

H264_NaluSave::H264_NaluSave() : process_queue(3), file_queue(40)
{
    thread_.set_loop_callback([this](SafeThread* self) ->bool{
        (void)self;

        std::string name;
        if(!file_queue.pop(name)){ LOG_DEBUG("debug"); return false;}

        VideoFramePtr frame;
        if(process_queue.timed_pop(frame, 2000)){
            if (frame && frame->data){
                std::ofstream outfile(name, std::ios::binary | std::ios::app);
                if (outfile.is_open()) {
                    outfile.write(reinterpret_cast<const char*>(frame->data->data()), frame->data->size());
                    outfile.close();
                }
            }
        }else{
            LOG_DEBUG("nalu save", "拍照失败，因为没有流");
            return false;
        }

        return true;
    });
}

void H264_NaluSave::create(H264_Encoder* input_source){
    input_source_ = input_source;
}

void H264_NaluSave::process_frames(VideoFramePtr frame){
    process_queue.push(frame);
}
bool H264_NaluSave::save(const std::string& name){
    bool ret = false;
    if(input_source_){input_source_->node_state.enable();}
    VideoFramePtr frame;
    if(process_queue.timed_pop(frame, 1000)){
        std::ofstream outfile(name, std::ios::binary | std::ios::app);
        if (outfile.is_open()) {
            outfile.write(reinterpret_cast<const char*>(frame->data->data()), frame->data->size());
            outfile.close();
        }
        ret = true;
    }else{
        LOG_DEBUG("H264_NaluSave", "拍照失败，因为没有流");
    }
    if(input_source_){input_source_->node_state.disable();}
    return ret;
}

