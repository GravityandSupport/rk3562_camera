#include "v4l2_nv12_capture.h"


int V4L2_NV12_Capture::register_device(){
    drm_buffers.reserve(buffer_num_);
    for (uint32_t i = 0; i < buffer_num_; ++i){
        DrmDumbBuffer* db = new DrmDumbBuffer();
        if (!db->create(drm_dev_.c_str(), width_, width_, 12)) { // nv12 相当于一个像素占12位数据
            std::cerr << "drm buffer create failed at idx=" << i << "\n";
            // 清理已创建的
            delete db;
            for (auto p : drm_buffers) delete p;
            return -1;
        }
        drm_buffers.push_back(db);
        LOG_DEBUG("drm buffer", i, db->get_dmabuf_fd(), db->size(), db->pitch());
    }

    ref_array_manage.setSize(drm_buffers.size());
    ref_array_manage.setOnZeroBackCall([&](int idx){
        this->queue_dmabuf(idx);
    });

    if (!cam->open_device(video_dev_.c_str())) {
        std::cerr << "open v4l2 failed\n";
        for (auto p : drm_buffers) delete p;
        return -1;
    }

    if (!cam->set_format(width_, height_, V4L2_PIX_FMT_NV12)) {
        std::cerr << "set_format failed\n";
        return -1;
    }

    if (!cam->req_buffer_dmabuf(buffer_num_)) {
        std::cerr << "req_buffer_dmabuf failed\n";
        return -1;
    }

    for (uint32_t i = 0; i < buffer_num_; ++i){
        queue_dmabuf(i);
    }

    return 0;
}
int V4L2_NV12_Capture::start_stream(){
    // 4) 启动流
    if (!cam->start_stream()) {
        std::cerr << "start_stream failed\n";
        return -1;
    }

    for(auto& _thread : thread_pool){
        _thread.set_loop_callback([this](SafeThread* self) ->bool{
            (void)self;
            int index=-1;
            if(!cam->ThreadSafeBoundedQueue::timed_pop(index, 2000)){
                LOG_DEBUG("取流队列", "timeout");
                return true;
            }

            this->ref_array_manage.acquire(index);

            //======================
            VideoBase::frames_ready(this, index);
//            LOG_DEBUG("CAPTURE", index);
            //======================

            auto time_id = TimerManager::getInstance().createTimer(std::chrono::milliseconds(30),
                                std::chrono::milliseconds(0),
            [index, this](TimerManager::TimerId i){
//                LOG_DEBUG("TIME", index, i);
                this->ref_array_manage.release(index); // 延迟一会再释放，因为防止有些节点申请比较晚，等早到到的时候这里已经释放了
                TimerManager::getInstance().destroyTimer(i);
            });
            TimerManager::getInstance().startTimer(time_id);
            return true;
        });
        _thread.start("V4L2_NV12_Capture");
    }

    std::clog << "Streaming started. Waiting for frames...\n";
    return 0;
}
void V4L2_NV12_Capture::stop_stream(){
    std::clog << "Stopping stream...\n";
    cam->stop_stream();

    for(auto& _thread : thread_pool){
        _thread.stop();
    }
}
void V4L2_NV12_Capture::uninstall_device(){
    // 清理：关闭 v4l2 设备，释放 drm buffers
    cam->close_device();
    for (auto p : drm_buffers) {
        delete p;
    }
    drm_buffers.clear();

    std::clog << "camera capture finished cleanly\n";
}
int V4L2_NV12_Capture::queue_dmabuf(uint32_t idx){
    if(idx>=drm_buffers.size()) { return -1;}
    int dmabuf_fd = drm_buffers[idx]->get_dmabuf_fd();
    uint32_t buf_size = drm_buffers[idx]->size();
    if (!cam->queue_dmabuf(idx, dmabuf_fd, buf_size)) {
        std::cerr << "queue_dmabuf failed idx=" << idx << "\n";
        return -1;
    }
    return 0;
}
DrmDumbBuffer* V4L2_NV12_Capture::acquire_dmabuf(uint32_t idx){
    if(idx>=drm_buffers.size()) { return nullptr;}
    this->ref_array_manage.acquire(idx);
    return drm_buffers[idx];
}
void V4L2_NV12_Capture::release_dmabuf(uint32_t idx){
    if(idx>=drm_buffers.size()) { return;}
    this->ref_array_manage.release(idx);
}
//bool V4L2_NV12_Capture::threadLoop(){
//    int index=-1;
//    if(!cam->ThreadSafeBoundedQueue::timed_pop(index, 2000)){
//        LOG_DEBUG("取流队列", "timeout");
//        return true;
//    }

//    this->ref_array_manage.acquire(index);

//    //======================
//    VideoBase::frames_ready(this, index);
//    LOG_DEBUG("CAPTURE", index);
//    //======================

//    this->ref_array_manage.release(index);

//    return true;
//}
