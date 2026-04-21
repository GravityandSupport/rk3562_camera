#include "v4l2camera.h"

bool V4L2Camera::open_device(const char* dev) {
    fd_ = ::open(dev, O_RDWR | O_NONBLOCK);
    if (fd_ < 0) {
        std::cerr << "open camera (" << dev << ") failed: " << strerror(errno) << "\n";
        return false;
    }

    dev_ = dev;
    LOG_DEBUG("V4L2", fd_);

    return true;
}
void V4L2Camera::close_device() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}
bool V4L2Camera::set_format(uint32_t w, uint32_t h, uint32_t pix_fmt) {
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = w;
    fmt.fmt.pix_mp.height = h;
    fmt.fmt.pix_mp.pixelformat = pix_fmt;
    fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
    fmt.fmt.pix_mp.num_planes = 1;

    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
        std::cerr << "VIDIOC_S_FMT failed: " << strerror(errno) << "\n";
        return false;
    }
    // read back for debug
    std::clog << "Set format: w=" << fmt.fmt.pix_mp.width << " h=" << fmt.fmt.pix_mp.height
              << " fmt=" << fmt.fmt.pix_mp.pixelformat << "\n";
    return true;
}

bool V4L2Camera::req_buffer_dmabuf(uint32_t count) {
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_DMABUF;

    if (ioctl(fd_, VIDIOC_REQBUFS, &req) < 0) {
        std::cerr << "VIDIOC_REQBUFS failed: " << strerror(errno) << "\n";
        return false;
    }
    buffers_count_ = req.count;
    return true;
}
bool V4L2Camera::queue_dmabuf(int index, int dma_fd, uint32_t buf_size) {
    if (index < 0) return false;
    struct v4l2_buffer buf;
    struct v4l2_plane plane;
    memset(&buf, 0, sizeof(buf));
    memset(&plane, 0, sizeof(plane));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.index = index;
    buf.length = 1;
    buf.m.planes = &plane;

    plane.m.fd = dma_fd;
    plane.length = buf_size;     // 必须与驱动期望一致（sizeimage）
    plane.data_offset = 0;

    if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
        std::cerr << "VIDIOC_QBUF idx=" << index << " failed: " << strerror(errno) << "\n";
        return false;
    }
    return true;
}
bool V4L2Camera::dequeue_dmabuf(int timeout_ms, uint32_t &out_bytesused, int &out_index) {
    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLIN | POLLPRI;
    pfd.revents = 0;

    int poll_ret = poll(&pfd, 1, timeout_ms);
    if (poll_ret < 0) {
        if (errno == EINTR) return false;
        std::cerr << "poll failed: " << strerror(errno) << "\n";
        return false;
    } else if (poll_ret == 0) {
        // timeout
        return false;
    }

    struct v4l2_buffer buf;
    struct v4l2_plane plane;
    memset(&buf, 0, sizeof(buf));
    memset(&plane, 0, sizeof(plane));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.length = 1;
    buf.m.planes = &plane;

    if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
        std::cerr << "VIDIOC_DQBUF failed: " << strerror(errno) << "\n";
        return false;
    }

    out_bytesused = plane.bytesused;
    out_index = buf.index;
    return true;
}
bool V4L2Camera::start_stream() {
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
        std::cerr << "VIDIOC_STREAMON failed: " << strerror(errno) << "\n";
        return false;
    }

    return true;
}

bool V4L2Camera::stop_stream() {
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd_, VIDIOC_STREAMOFF, &type) < 0) {
        std::cerr << "VIDIOC_STREAMOFF failed: " << strerror(errno) << "\n";
        return false;
    }

    return true;
}









