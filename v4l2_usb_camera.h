#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "videobase.h"
#include "safe_thread.h"

class V4l2USBCamera : public VideoBase
{
public:
    bool open_device(const char* dev);
    void close_device();

    bool set_format(uint32_t w, uint32_t h);
    bool req_buffer_dmabuf(uint32_t count);
    bool queueBuffers(uint32_t index);
    bool dequeueBuffers(int timeout_ms, uint32_t &out_bytesused, int &out_index);

    bool start_stream();
    bool stop_stream();

    bool register_device(uint32_t width, uint32_t height, uint32_t buffer_num, const std::string& video_dev);
    bool uninstall_device();

    virtual ~V4l2USBCamera();

private:
    std::string dev_;
    int width_;
    int height_;
    int fd_;
    std::vector<USB_Buffer> buffers_;

    SafeThread thread_;
};