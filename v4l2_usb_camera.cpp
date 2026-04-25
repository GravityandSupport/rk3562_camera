#include "v4l2_usb_camera.h"
#include "outLog.h"
#include <poll.h>

V4l2USBCamera::~V4l2USBCamera(){

}

bool V4l2USBCamera::open_device(const char* dev) {
    fd_ = ::open(dev, O_RDWR | O_NONBLOCK);
    if (fd_ < 0) {
        std::cerr << "open camera (" << dev << ") failed: " << strerror(errno) << "\n";
        return false;
    }

    dev_ = dev;
    LOG_DEBUG("V4L2", fd_);

    return true;
}
void V4l2USBCamera::close_device() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}
bool V4l2USBCamera::set_format(uint32_t w, uint32_t h){
    width_=w;
    height_=h;

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width_;
    fmt.fmt.pix.height = height_;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG; // MJPG
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
        perror("VIDIOC_S_FMT");
        return false;
    }

    return true;
}
bool V4l2USBCamera::req_buffer_dmabuf(uint32_t count) {
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));

    req.count = count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd_, VIDIOC_REQBUFS, &req) < 0) {
        perror("VIDIOC_REQBUFS");
        return false;
    }

    buffers_.resize(req.count);

    for (size_t i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("VIDIOC_QUERYBUF");
            return false;
        }

        buffers_[i].length = buf.length;
        buffers_[i].start = mmap(NULL, buf.length,
                                    PROT_READ | PROT_WRITE,
                                    MAP_SHARED,
                                    fd_, buf.m.offset);

        if (buffers_[i].start == MAP_FAILED) {
            perror("mmap");
            return false;
        }
    }

    return true;
}
bool V4l2USBCamera::queueBuffers(uint32_t index){
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;

    if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
        perror("VIDIOC_QBUF");
        return false;
    }
    return true;
}
bool V4l2USBCamera::dequeueBuffers(int timeout_ms, uint32_t &out_bytesused, int &out_index){
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
    memset(&buf, 0, sizeof(buf));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
        perror("VIDIOC_DQBUF");
        return false;
    }

//    void* data = buffers_[buf.index].start;
//    size_t size = buf.bytesused;
    // LOG_DEBUG("usb camera", data, size, buf.index);

    out_bytesused = buf.bytesused;
    out_index = buf.index;

    return true;
}

bool V4l2USBCamera::start_stream(){
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
        return false;
    }
    
    return true;
}
bool V4l2USBCamera::stop_stream(){
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd_, VIDIOC_STREAMOFF, &type) < 0) {
        std::cerr << "VIDIOC_STREAMOFF failed: " << strerror(errno) << "\n";
        return false;
    }

    return true;
}


bool V4l2USBCamera::register_device(uint32_t width, uint32_t height, uint32_t buffer_num, const std::string& video_dev){
    if(!open_device(video_dev.c_str())){
        LOG_DEBUG("USB CAMERA", "open v4l2 failed");
        return false;
    }

    if(!set_format(width, height)){
        LOG_DEBUG("USB CAMERA", "set_format failed");
        return false;
    }

    if(!req_buffer_dmabuf(buffer_num)){
        LOG_DEBUG("USB CAMERA", "req_buffer_dmabuf failed");
        return false;
    }

    for(size_t i=0; i<buffers_.size(); i++){
        queueBuffers(i);
    }

    if(!start_stream()){
        LOG_DEBUG("USB CAMERA", "start_stream failed");
        return false;
    }

    thread_.set_loop_callback([this](SafeThread* self) ->bool{
        (void)self;

        uint32_t bytesused = 0;
        int index = -1;
        if(!dequeueBuffers(3000, bytesused, index)){
            LOG_DEBUG("CAMERA", "USB Camera no frame (timeout or interrupted)");
            return true;
        }

        // LOG_DEBUG("USB CAMERA", index);
        VideoDrmBufPtr frame = std::make_shared<VideoDrmBuf>();
        frame->usb_buffer.start = buffers_[index].start;
        frame->usb_buffer.length = bytesused;
        frame->usb_buffer.width = width_;
        frame->usb_buffer.height = height_;
        frames_ready(frame);
        
        queueBuffers(index);


        return true;
    });
    thread_.start("h264 encoder");

    return true;
}

bool V4l2USBCamera::uninstall_device(){
    thread_.stop();

    if(!stop_stream()){
        LOG_DEBUG("USB CAMERA", "stop_stream failed");
        return false;
    }

    for (auto& buf : buffers_) {
        munmap(buf.start, buf.length);
    }
    buffers_.clear();

    close_device();

    std::clog << "camera capture finished cleanly\n";
    return true;
}
