#pragma once

#include <vector>
#include <memory>
#include <list>
#include <mutex>
#include <array>

#include "PoolBuffer.h"
#include "VideoNodeState.h"

class DrmDumbBuffer;
class VideoBase;

struct NV12_Packet{
    VideoBase* capture;
    int idx;
};

struct USB_Buffer{
    void* start=nullptr;
    size_t length=0;

    int width=0;
    int height=0;
};

class VideoBase
{
public:
    explicit VideoBase();
    virtual ~VideoBase();

    VideoBase* parent_node = nullptr;

    // ================== 通道定义 ==================
    enum class ChannelType : int {
        NV12_INDEX = 0,
        FRAME_DATA,
        FRAME_DRMBUF,
        // 后续扩展
        MAX
    };

    // ================== 处理接口 ==================
    virtual void process_frames(VideoBase* capture, int idx){(void)capture;(void)idx;}
    virtual void frames_ready(VideoBase* capture, int idx);

    struct VideoFrame {
        uint32_t width;     // 宽
        uint32_t height;    // 高

        uint32_t stride;    // 行对齐（很重要，后面你一定会用到）

        std::shared_ptr<std::vector<uint8_t>> data;
    };
    using VideoFramePtr = std::shared_ptr<VideoFrame>;
    virtual void process_frames(VideoFramePtr frame){(void)frame;}
    virtual void frames_ready(VideoFramePtr frame);
    /*        子类实现的具体处理逻辑示例代码
    virtual void process_frames(VideoFramePtr frame) {
        if (!frame || frame->empty()) return;

        // 访问数据的方式：
        // uint8_t* raw_data = frame->data();
        // size_t size = frame->size();
    }
     */

    struct VideoDrmBuf {
        VideoBase* video=nullptr;
        DrmDumbBuffer* buffer=nullptr;
        ISlot* slot = nullptr;

        USB_Buffer usb_buffer; // usb摄像头出流专属
    };
    using VideoDrmBufPtr = std::shared_ptr<VideoDrmBuf>;
    virtual void process_frames(VideoDrmBufPtr frame){(void)frame;}
    virtual void frames_ready(VideoDrmBufPtr frame);

    void add_video(VideoBase* _video);
    void remove_video(VideoBase* video);
    void set_enable(VideoBase* video, ChannelType channel, bool enable);
private:
    std::mutex mtx_;

    struct VideoNode {
        VideoBase* node;
        std::array<bool, static_cast<int>(ChannelType::MAX)> enable_array_;
    };
    std::list<VideoNode> video_list;
};

