#include "drmdumbbuffer.h"

#include "outLog.h"

int DrmDumbBuffer::buffer_size = 0;

bool DrmDumbBuffer::create(uint32_t w, uint32_t h, uint32_t bpp){
    return create("/dev/dri/card0", w, h, bpp);
}

bool DrmDumbBuffer::create(const char* dev, uint32_t w, uint32_t h, uint32_t bpp) {
    width_ = w; height_ = h;bpp_=bpp;

    fd_ = open(dev, O_RDWR | O_CLOEXEC);
    if (fd_ < 0) {
        std::cerr << "open drm (" << dev << ") failed: " << strerror(errno) << "\n";
        return false;
    }

    struct drm_mode_create_dumb req;
    memset(&req, 0, sizeof(req));
    req.width = width_;
    req.height = height_*bpp/8;
    req.bpp = 8;

    if (drmIoctl(fd_, DRM_IOCTL_MODE_CREATE_DUMB, &req) < 0) {
        std::cerr << "DRM_IOCTL_MODE_CREATE_DUMB failed: " << strerror(errno) << "\n";
        return false;
    }

    handle_ = req.handle;
    pitch_ = req.pitch;
    size_  = req.size;

    // export to dma-buf fd
    struct drm_prime_handle prime;
    memset(&prime, 0, sizeof(prime));
    prime.handle = handle_;
    prime.flags = DRM_CLOEXEC | DRM_RDWR;

    if (drmIoctl(fd_, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime) < 0) {
        std::cerr << "DRM_IOCTL_PRIME_HANDLE_TO_FD failed: " << strerror(errno) << "\n";
        return false;
    }

    dmabuf_fd_ = prime.fd;

    // get mmap offset
    struct drm_mode_map_dumb map_req;
    memset(&map_req, 0, sizeof(map_req));
    map_req.handle = handle_;
    if (drmIoctl(fd_, DRM_IOCTL_MODE_MAP_DUMB, &map_req) < 0) {
        std::cerr << "DRM_IOCTL_MODE_MAP_DUMB failed: " << strerror(errno) << "\n";
        return false;
    }

    map_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, map_req.offset);
    if (map_ == MAP_FAILED) {
        map_ = nullptr;
        std::cerr << "mmap failed: " << strerror(errno) << "\n";
        return false;
    }

    // zero the buffer (optional but useful)
    memset(map_, 0x10, size_);

    buffer_size++;
    LOG_DEBUG("drm buffer", buffer_size,  fd_, width_, height_, getSize());

    return true;
}

void DrmDumbBuffer::destroy() {
    if (map_) {
        munmap(map_, size_);
        map_ = nullptr;
    }
    if (dmabuf_fd_ >= 0) {
        close(dmabuf_fd_);
        dmabuf_fd_ = -1;
    }
    if (handle_) {
        struct drm_mode_destroy_dumb d;
        memset(&d, 0, sizeof(d));
        d.handle = handle_;
        drmIoctl(fd_, DRM_IOCTL_MODE_DESTROY_DUMB, &d);
        handle_ = 0;
    }
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
    buffer_size--;
}
