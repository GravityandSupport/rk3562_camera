#pragma once

#include "safe_thread.h"
#include <libudev.h>
#include <iostream>
#include <fcntl.h>      // open
#include <sys/ioctl.h>  // ioctl
#include <linux/videodev2.h>
#include <unistd.h>     // close
#include <cstring>
#include <sys/select.h>
#include <memory>
#include <map>

#include "v4l2_usb_camera.h"
#include "mjpeg_decoder.h"
#include "videomerge.h"

class UVC_Monitor{
public:
    void start();
    void stop();

    virtual ~UVC_Monitor();
private:
    int fd = -1;
    struct udev *udev = nullptr;
    struct udev_monitor *mon = nullptr;

    SafeThread thread_;

    std::map<std::string, std::shared_ptr<V4l2USBCamera>> cameras_;

    const VideoBase* video_merge_node;

    bool supportsMJPG(const char* dev_name);
};
