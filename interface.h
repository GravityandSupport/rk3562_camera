#ifndef INTERFACE_H
#define INTERFACE_H

#include "v4l2_nv12_capture.h"
#include "h264_encoder.h"
#include "timermanager.h"
#include "tcp_server.h"
#include "tcpclient.h"
#include "videomerge.h"

extern std::shared_ptr<V4L2_NV12_Capture> capture;
extern H264_Encoder h264_encoder;

extern TcpServer tcp_server;

extern TcpClient tcp_client;

extern VideoMerge video_merge;

#endif // INTERFACE_H
