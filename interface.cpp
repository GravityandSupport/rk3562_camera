#include "interface.h"


std::shared_ptr<V4L2_NV12_Capture> capture;
H264_Encoder h264_encoder;

TcpServer tcp_server;

TcpClient tcp_client;

VideoMerge video_merge;
