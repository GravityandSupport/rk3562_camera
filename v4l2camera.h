#ifndef V4L2CAMERA_H
#define V4L2CAMERA_H

#include "drmdumbbuffer.h"
#include "safe_thread.h"

#include "ThreadSafeBoundedQueue.h"

class V4L2Camera
{
public:
//    V4L2Camera() = default;
    V4L2Camera() : fd_(-1), buffers_count_(0) {}
    virtual ~V4L2Camera() { close_device(); }

    bool open_device(const char* dev);
    void close_device();

    bool set_format(uint32_t w, uint32_t h, uint32_t pix_fmt = V4L2_PIX_FMT_NV12);
    bool req_buffer_dmabuf(uint32_t count);
    bool queue_dmabuf(int index, int dma_fd, uint32_t buf_size);
    bool dequeue_dmabuf(int timeout_ms, uint32_t &out_bytesused, int &out_index);

    bool start_stream();
    bool stop_stream();

private:
    int fd_;
    uint32_t buffers_count_=10;
    std::string dev_;

};

#endif // V4L2CAMERA_H
