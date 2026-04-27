#ifndef DRMDUMBBUFFER_H
#define DRMDUMBBUFFER_H


#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <sys/time.h>

#include <linux/videodev2.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "outLog.h"

class DrmDumbBuffer {
public:
    DrmDumbBuffer() :
        fd_(-1), handle_(0), pitch_(0),
        size_(0), map_(nullptr), dmabuf_fd_(-1),
        width_(0), height_(0) {}

    virtual ~DrmDumbBuffer() { destroy(); }

    bool create(const char* dev, uint32_t w, uint32_t h, uint32_t bpp = 32);
    bool create(uint32_t w, uint32_t h, uint32_t bpp = 32);

    int get_dmabuf_fd() const { return dmabuf_fd_; }
    uint32_t pitch() const { return pitch_; }
    void* map() const { return map_; }
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    uint32_t size() const { return size_; }

    // 一些特殊情况，比如mjpeg解码后，并不会使用使用全部的内存空间，所以图像的宽高和内存的宽高并不匹配
    void setWidth(uint32_t width) { width_ = width;}
    void setHeight(uint32_t height) { height_ = height;}
    void setBpp(uint32_t bpp) {bpp_ = bpp; }
    uint32_t bytesused() { return bytesused_;}
    void bytesused(uint32_t _bytesused) {bytesused_ = _bytesused;}

    static int buffer_size;
private:
    void destroy();

private:
    int fd_;
    uint32_t handle_;
    uint32_t pitch_;
    uint32_t size_;
    void* map_;
    int dmabuf_fd_;
    uint32_t width_, height_;
    uint32_t bpp_;
    uint32_t bytesused_;
};

#endif // DRMDUMBBUFFER_H
