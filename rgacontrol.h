#ifndef RGACONTROL_H
#define RGACONTROL_H

#include "drmdumbbuffer.h"

#include "rga/RgaUtils.h"
#include "rga/im2d.hpp"

class RgaControl
{
public:
    RgaControl();

    enum class Format{
        NV12,
    };

    static RgaSURF_FORMAT to_RKRgaFormat(Format);

    static bool resize_rect(DrmDumbBuffer* src, DrmDumbBuffer* dst, Format src_format, Format dst_format, im_rect rect);
    static bool copy(DrmDumbBuffer* src, DrmDumbBuffer* dst, Format src_format, Format dst_format);
    static bool resize(DrmDumbBuffer* src, DrmDumbBuffer* dst, Format src_format, Format dst_format);
    static void test();
};

#endif // RGACONTROL_H
