#ifndef INTERFACE_H
#define INTERFACE_H

#include "v4l2_nv12_capture.h"
#include "h264_encoder.h"
#include "timermanager.h"
#include "tcp_server.h"
#include "tcpclient.h"
#include "videomerge.h"
#include "DmaBufRenderer.h"
#include "ImageDisplay.h"
#include "v4l2_usb_camera.h"
#include "mjpeg_decoder.h"
#include "uvc_monitor.h"
#include "mjpeg_encoder.h"
#include  "h264_nalusave.h"
#include "JsonWrapper.h"

extern V4L2_NV12_Capture capture;
extern V4L2_NV12_Capture capture_33;
extern H264_Encoder h264_encoder;

extern TcpServer tcp_server;

extern TcpClient tcp_client;

extern VideoMerge video_merge;

extern ImageDisplay image_display;

extern V4l2USBCamera usb_camera;
extern MJPEG_Decoder mjpeg_decoder;
extern UVC_Monitor uvc_monitor;

extern MJPEG_Encoder mjpeg_encoder;

extern H264_NaluSave h264_nalu_save;

#endif // INTERFACE_H
