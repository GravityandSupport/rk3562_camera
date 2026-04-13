#pragma once

#include <QObject>
#include <vector>
#include <memory>
#include <list>
#include <mutex>
#include <array>

class VideoBase;

struct NV12_Packet{
    VideoBase* capture;
    int idx;
};

class VideoBase : public QObject
{
    Q_OBJECT
public:
    explicit VideoBase(QObject *parent = nullptr);
    virtual ~VideoBase() = default;

    // ================== 通道定义 ==================
    enum class ChannelType : int {
        NV12_INDEX = 0,
        FRAME_DATA,
        // 后续扩展
        MAX
    };

    // ================== 通道控制接口 ==================
    void enable_channel(ChannelType type, bool enable);
    bool is_channel_enabled(ChannelType type);
    bool is_channel_enabled_nolock(ChannelType type);

    // ================== 处理接口 ==================
    virtual void process_frames(VideoBase* capture, int idx){(void)capture;(void)idx;}
    virtual void frames_ready(VideoBase* capture, int idx);

    using VideoFramePtr = std::shared_ptr<std::vector<uint8_t>>;
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

    void add_video(VideoBase* _video);

private:
    std::list<VideoBase*> video_list;
    std::mutex mtx_;

    // 通道控制
    std::array<bool, static_cast<int>(ChannelType::MAX)> channel_array_; // 输入路径控制，表示该节点支持哪个路径的数据流入
};

