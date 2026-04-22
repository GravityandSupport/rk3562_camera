# rk3562_camera
RK3562运动相机开发

# 软件流程
```mermaid
flowchart TD
    subgraph 主窗口初始化
        direction TD
        软件定时器初始化-->tcp服务器启动
        tcp服务器启动-->udp启动
        udp启动-->video22启动
        video22启动-->video22_capture启动
        video22_capture启动-->video33启动
        video33启动-->video33_capture启动
        video33_capture启动-->h264编码器启动
    end

    subgraph nv12捕获线程池
        direction TD
        摄像头poll等待-->缓冲区出队
        缓冲区出队-->缓冲区入队
    end
    video22_capture启动-->nv12捕获线程池
    video33_capture启动-->nv12捕获线程池

    subgraph 图像合并
        缓冲区出队-->|两路输入|图像合并两路输入
        图像合并两路输入-->图像合并缓冲池取一帧[从缓冲池中取出一帧，帧引用次数+1 缓冲池自动维护大概10个缓冲对象]
        图像合并缓冲池取一帧-->图像合并输出[输出]
        图像合并输出-->图像合并归还缓冲池一帧[帧引用次数-1 如果引用次数归0 归还缓冲池]
    end

    subgraph h264编码线程
        direction TD
        图像合并输出-->h264编码接收输入
        h264编码接收输入-->h264编码完成
    end
    h264编码器启动-->h264编码线程

    subgraph ffmpeg封装线程
        direction TD
        h264编码完成-->ffmpeg封装输入
        ffmpeg封装输入-->ffmpeg封装输出
        ffmpeg封装输出-->ffmpeg封装保存为MP4文件[保存为MP4文件]
    end

    subgraph JPEG编码线程
        direction TD
        图像合并输出-->JPEG编码收输入
        JPEG编码收输入-->JPEG编码完成
    end


    subgraph tcp服务器线程
        direction TD
        tcp_epoll等待-->tcp有新连接
        tcp有新连接[tcp每1秒发送识别码,等待从机发送类型代码]
        tcp有新连接-->tcp识别成功[tcp类型识别成功,实例化对应类对象,启动心跳检测,每5秒发送心跳包]
        tcp识别成功-->tcp服务器数据输出

        tcp_epoll等待-->tcp从机有数据过来
        tcp从机有数据过来-->tcp_queue_push[tcp queue_push]

        tcp数据传输至接收端-->|将数据传输至对应的派生类|tcp服务器数据输出
        subgraph tcp微型线程池
            direction TD
            tcp_queue_pop[tcp queue_pop]-->tcp数据传输至接收端
        end
    end
    tcp_queue_push-->tcp_queue_pop
    tcp服务器启动-->tcp服务器线程

    subgraph pc端udp图传派生类
        direction TD
        pc端udp图传发送询问端口号[询问从机udp端口号]-->pc端udp图传h264裸流发送[将h264裸流数据分包处理后通过ip+port唯一id标识发送至从机]
    end
    tcp服务器数据输出-->pc端udp图传派生类
    h264编码完成-->pc端udp图传h264裸流发送

```

# TCP通讯协议
## 通讯协议格式说明
| 字段 | 长度 | 值/范围 | 说明 |
|:-----|:-----|:--------|:-----|
| 帧头1 | 1 字节 | `0x5A` | 帧起始标志字节1 |
| 帧头2 | 1 字节 | `0xA5` | 帧起始标志字节2 |
| Addr | 2 字节 | `0x0000 - 0xFFFF` | 目标地址（16位无符号整数） |
| Len | 2 字节 | `0x0000 - 0xFFFF` | 数据域长度（16位无符号整数） |
| Data | Len 字节 | 任意数据 | 实际传输的有效数据内容 |

**完整帧结构示例**

| 帧头1 | 帧头2 | Addr (高字节) | Addr (低字节) | Len (高字节) | Len (低字节) | Data[0] | ... | Data[Len-1] |
|:-----:|:-----:|:------------:|:-------------:|:-----------:|:------------:|:-------:|:---:|:-----------:|
| 0x5A  | 0xA5  | ADDR_H       | ADDR_L        | LEN_H       | LEN_L        | DATA_0  | ... | DATA_N      |

**数据解析说明**

> **注意**：Len 字段表示 Data 域的实际字节长度，接收端应根据 Len 值读取相应数量的数据字节。

**示例**
- 发送地址 `0x1234`，数据长度 `5` 字节，数据内容为 `[0x01, 0x02, 0x03, 0x04, 0x05]`

| 0x5A | 0xA5 | 0x12 | 0x34 | 0x00 | 0x05 | 0x01 | 0x02 | 0x03 | 0x04 | 0x05 |

## 寄存器地址功能定义表
### 通用寄存器功能定义表

| 寄存器地址 (Addr) | 功能描述 | 数据长度 (Len) | 数据方向 | 说明 |
|:----------------:|:---------|:--------------:|:--------------:|:-----|
| `0xa000` | 主机访问从机类型 | 0 | 主→从 | 无 |
| `0xa0a0` | 主机发送心跳包/从机回复心跳包 | 0 |  主→从/ 从→主 |双向数据 |

### PC端图传类寄存器功能定义表

| 寄存器地址 (Addr) | 功能描述 | 数据长度 (Len) | 数据方向 | 说明 |
|:----------------:|:---------|:--------------:|:--------------:|:-----|
| `0x4000` | 主机访问udp端口号 | 0 |  主→从 |无 |
| `0x4000` | 从机回复udp端口号 | 2 |  从→主 |高位在前 |
