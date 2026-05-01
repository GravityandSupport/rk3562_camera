#include "h264_nalusave.h"
#include <fstream>
#include <iomanip>
#include "outLog.h"

#include "JsonWrapper.h"

H264_NaluSave::H264_NaluSave() : process_queue(3), file_queue(20)
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

void H264_NaluSave::create(){
    impl_device = std::make_shared<ImplDevice>();
    impl_device->subscribe("/video/h264/nalu_save");
    impl_device->instance = this;
}

void H264_NaluSave::process_frames(VideoFramePtr frame){
    process_queue.push(frame);
}

void H264_NaluSave::ImplDevice::onMessage(const EventMsg& msg){
    JsonWrapper js(msg.payload);
    std::vector<std::string> filename;
    if(js.get("filename", filename)){
        if(instance){
            for(auto& name : filename){
                instance->file_queue.push(name);
            }
            instance->thread_.start();
        }
    }
}
