#include "uvc_monitor.h"

#include "outLog.h"

#include "interface.h"

UVC_Monitor::~UVC_Monitor(){}

void UVC_Monitor::start(){
    udev = udev_new();
    if (!udev) {
        std::cerr << "udev_new failed\n";
        return ;
    }

    mon = udev_monitor_new_from_netlink(udev, "udev");
    if (!mon) {
        std::cerr << "udev_monitor_new_from_netlink failed\n";
        return ;
    }

    // 只监听 video4linux（摄像头）
    udev_monitor_filter_add_match_subsystem_devtype(mon, "video4linux", NULL);
    udev_monitor_enable_receiving(mon);

    fd = udev_monitor_get_fd(mon);

    thread_.set_loop_callback([this](SafeThread* self) ->bool{
        (void)self;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        int ret = select(fd + 1, &fds, NULL, NULL, NULL);
        if (ret > 0 && FD_ISSET(fd, &fds)) {

            struct udev_device *dev = udev_monitor_receive_device(mon);
            if (dev) {
                const char *action = udev_device_get_action(dev); // add/remove
                const char *devnode = udev_device_get_devnode(dev); // /dev/videoX

                if (action && devnode) {
                    std::cout << "事件: " << action
                              << " 设备: " << devnode << std::endl;
                }
                
                if(std::string(action)=="add"){
                    LOG_DEBUG("uvc monitor", std::string(action), std::string(devnode));
                    if(supportsMJPG(devnode)){
                        std::shared_ptr<V4l2USBCamera> camera = std::make_shared<V4l2USBCamera>();
                        camera->add_video(&mjpeg_decoder);
                        mjpeg_decoder.add_video(&video_merge);
                        video_merge_node = video_merge.setBigNode(&mjpeg_decoder, 0, 0, 1024, 592);

                        camera->register_device(1920, 1072, 2, devnode);
                        cameras_[devnode] = camera;
                    }else{
                        LOG_DEBUG("uvc monitor", "该设备不支持MJPEG格式输出，跳过");
                    }
                }else if(std::string(action)=="remove"){
                    LOG_DEBUG("uvc monitor", std::string(action), std::string(devnode));
                    auto it = cameras_.find(devnode);
                    if (it != cameras_.end()){
                        std::shared_ptr<V4l2USBCamera> camera = it->second;
                        mjpeg_decoder.remove_video(&video_merge);
                        camera->uninstall_device();

                        video_merge.setBigNode(video_merge_node.video, 0, 0, 1024, 592);

                        cameras_.erase(it);
                    }
                } 

                udev_device_unref(dev);
            }
        }

        return true;
    });
    thread_.start("uvc_monitor");
}

void UVC_Monitor::stop(){

}

bool UVC_Monitor::supportsMJPG(const char* dev_name){
    // 1. 打开设备
    // const char* dev_name = "/dev/video0";
    int fd = open(dev_name, O_RDWR);
    if (fd == -1) {
        perror("Opening video device");
        return 1;
    }

    // 2. 准备枚举结构体
    struct v4l2_fmtdesc fmtdesc;
    memset(&fmtdesc, 0, sizeof(fmtdesc));
    
    // 指定我们要查询的类型：视频采集
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;

    std::cout << "Supported formats for " << dev_name << ":" << std::endl;
    std::cout << "--------------------------------------------" << std::endl;

    // 3. 循环获取所有支持的格式
    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        // 打印 FourCC 编码（4字节标识符）和 描述
        printf("[%d] ID: %c%c%c%c, Description: %s\n", 
               fmtdesc.index,
               fmtdesc.pixelformat & 0xFF, 
               (fmtdesc.pixelformat >> 8) & 0xFF,
               (fmtdesc.pixelformat >> 16) & 0xFF, 
               (fmtdesc.pixelformat >> 24) & 0xFF,
               fmtdesc.description);
        
        fmtdesc.index++;

        if(fmtdesc.pixelformat==V4L2_PIX_FMT_MJPEG){
            LOG_DEBUG("uvc monitor", "支持MJPEG格式输出");
            return true;
        }
    }

    // 4. 关闭设备
    close(fd);
    return false;
}

