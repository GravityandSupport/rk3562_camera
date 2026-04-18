#include "rgacontrol.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <linux/stddef.h>

#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>

#include "outLog.h"

#define LOG_TAG "rga"

RgaControl::RgaControl()
{

}
RgaSURF_FORMAT RgaControl::to_RKRgaFormat(Format format){
    switch (format) {
    case Format::NV12: return RK_FORMAT_YCbCr_420_SP;
    default:break;
    }

    return RK_FORMAT_YCbCr_420_SP;
}
bool RgaControl::resize_rect(DrmDumbBuffer* src_drm, DrmDumbBuffer* dst_drm, Format _src_format, Format _dst_format, im_rect rect){
    int ret;
    int src_width, src_height, src_format;
    int dst_width, dst_height, dst_format;

    rga_buffer_handle_t src_handle, dst_handle;
    rga_buffer_t src = {};
    rga_buffer_t dst = {};

    src_width = src_drm->width();
    src_height = src_drm->height();
    src_format = to_RKRgaFormat(_src_format);

    dst_width = dst_drm->width();
    dst_height = dst_drm->height();
    dst_format = to_RKRgaFormat(_dst_format);

    src_handle = importbuffer_fd(src_drm->get_dmabuf_fd(), src_drm->size());
    dst_handle = importbuffer_fd(dst_drm->get_dmabuf_fd(), dst_drm->size());

    src = wrapbuffer_handle(src_handle, src_width, src_height, src_format);
    dst = wrapbuffer_handle(dst_handle, dst_width, dst_height, dst_format);

    ret = imcheck(src, dst, {}, {});
    if (IM_STATUS_NOERROR != ret) {
        printf("%d, check error! %s", __LINE__, imStrError((IM_STATUS)ret));
        goto release_buffer;
    }

    ret = improcess(src, dst, {}, {}, rect, {}, IM_SYNC);
    if (ret == IM_STATUS_SUCCESS) {
        printf("%s running success!\n", LOG_TAG);
    } else {
        printf("%s running failed, %s\n", LOG_TAG, imStrError((IM_STATUS)ret));
        goto release_buffer;
    }

    return true;

release_buffer:
    if (src_handle > 0)
        releasebuffer_handle(src_handle);
    if (dst_handle > 0)
        releasebuffer_handle(dst_handle);

    return false;
}

void RgaControl::test(){
    DrmDumbBuffer src_drm, dst_drm;
    src_drm.create("/dev/dri/card0", 1024, 592, 12);
    dst_drm.create("/dev/dri/card0", 1920, 1072, 12);

    size_t total_size = src_drm.getSize();
    std::ifstream ifs("/mnt/nfs_dir/nv12.yuv", std::ios::in | std::ios::binary);
    if (!ifs.is_open()) {
        std::cerr << "错误：无法打开文件 " << "/mnt/nfs_dir/nv12.yuv" << std::endl;
        return;
    }

    // 检查文件大小是否匹配（可选，但推荐）
    ifs.seekg(0, std::ios::end);
    size_t file_size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    if (file_size < total_size) {
        std::cerr << "警告：文件大小 (" << file_size << ") 小于预期的 NV12 大小 ("
                  << total_size << ")" << std::endl;
        // 根据业务需求决定是否继续读取
    }

    LOG_DEBUG("RGA", file_size, total_size);

    // 执行读取操作
    ifs.read(reinterpret_cast<char*>(src_drm.map()), total_size);

    // 检查实际读取了多少字节
    std::streamsize bytes_read = ifs.gcount();
    std::cout << "bytes_read=" << bytes_read << std::endl;

    ifs.close();

    im_rect rect;
    rect.x = 200;
    rect.y = 50;
    rect.width = 1024;
    rect.height = 592;
    if(!resize_rect(&src_drm, &dst_drm, Format::NV12, Format::NV12, rect)){
        std::cerr << "转换失败\n";
        return;;
    }
    rect.x = 500;
    rect.y = 400;
    rect.width = 1024;
    rect.height = 592;
    if(!resize_rect(&src_drm, &dst_drm, Format::NV12, Format::NV12, rect)){
        std::cerr << "转换失败\n";
        return;;
    }

    // 使用二进制模式打开文件
    std::ofstream ofs("/mnt/nfs_dir/nv12_out.yuv", std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open file for writing: " << "/mnt/nfs_dir/nv12_out.yuv" << std::endl;
        return ;
    }

    // 写入整个数据块
    ofs.write(reinterpret_cast<const char*>(dst_drm.map()), dst_drm.getSize());
    ofs.flush(); // 刷新流缓冲区
    ofs.close();

    LOG_DEBUG("rga", src_drm.size(), dst_drm.size());
}

