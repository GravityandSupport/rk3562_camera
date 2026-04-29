#include "h264_nalusave.h"
#include <fstream>
#include <iomanip>
#include "outLog.h"

H264_NaluSave::H264_NaluSave() : process_queue(3)
{
    thread_.set_loop_callback([this](SafeThread* self) ->bool{
        (void)self;

        if(number_of_photos_taken.load(std::memory_order_relaxed)<=0){
            enable_switch.store(false, std::memory_order_relaxed);
            return false;
        }

        VideoFramePtr frame;
        if(process_queue.timed_pop(frame, 2000)){
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm bt = *std::localtime(&in_time_t);

            std::stringstream ss;
            ss << "/mnt/nfs_dir/" << std::put_time(&bt, "%Y-%m-%d-%S%M") << "-" << number_of_photos_taken.load(std::memory_order_relaxed) << ".h264";
            std::string filename = ss.str();

            LOG_DEBUG("h264 nalu", filename);
            if (frame && frame->data) {
                std::ofstream outfile(filename, std::ios::binary | std::ios::app);
                if (outfile.is_open()) {
                    // 注意：这里直接写入了整个 vector。
                    // 如果存在 stride 填充，通常 H264 裸流不需要处理 stride，
                    // 但如果是编码前的像素数据，则需要逐行按 stride 写入。
                    outfile.write(reinterpret_cast<const char*>(frame->data->data()), frame->data->size());
                    outfile.close();

                    // 4. 递减计数器
                    number_of_photos_taken.fetch_sub(1, std::memory_order_relaxed);
                }
            }
        }else{

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
    if(enable_switch.load(std::memory_order_relaxed)){
        process_queue.push(frame);
    }
}

void H264_NaluSave::ImplDevice::onMessage(const EventMsg& msg){
    try {
        auto j = nlohmann::json::parse(msg.payload);
        std::cout << j.dump(4) << std::endl;
        if (j.contains("frame_num")){
            int num = j.at("frame_num").get<int>();
            if(instance){
                instance->number_of_photos_taken.store(num, std::memory_order_relaxed);
                instance->thread_.start();
                instance->enable_switch.store(true, std::memory_order_relaxed);
            }
        }
    } catch (const std::exception& e){

    }
}
