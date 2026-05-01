#include "interface.h"


V4L2_NV12_Capture capture;
V4L2_NV12_Capture capture_33;

H264_Encoder h264_encoder;

TcpServer tcp_server;

TcpClient tcp_client;

VideoMerge video_merge;

ImageDisplay image_display;

V4l2USBCamera usb_camera;
MJPEG_Decoder mjpeg_decoder;
UVC_Monitor uvc_monitor;

H264_NaluSave h264_nalu_save;

MJPEG_Encoder mjpeg_encoder;

H264_Decoder h264_decoder;
