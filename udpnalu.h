#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <map>
#include <unordered_set>

// POSIX socket
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "udpsocket.h"
#include "ThreadSafeBoundedQueue.h"
#include "safe_thread.h"

struct PktHeader;

class UdpNalu {
private:
    struct NALU {
        const uint8_t* data;
        size_t         size;
    };

    // ─── 单个 NALU 的分片重组器 ───────────────────────────────────────────────
    struct FrameAssembler {
        uint32_t    frame_id;
        uint16_t    frag_total;
//        uint8_t     nalu_type;
        double      created_at;                         // steady_clock 秒
        std::map<uint16_t, std::vector<uint8_t>> frags; // index -> payload

        FrameAssembler() = default;
        FrameAssembler(uint32_t fid, uint16_t total, /*uint8_t nt,*/ double now)
            : frame_id(fid), frag_total(total), /*nalu_type(nt),*/ created_at(now) {}

        bool is_complete() const;
        bool is_expired(double now) const ;

        // 按序拼接所有分片
        std::vector<uint8_t> assemble() const ;
    };

    // ─── 完整 NALU 队列元素 ───────────────────────────────────────────────────
    struct CompleteNALU {
        uint32_t             frame_id;
        uint8_t              nalu_type;
        std::vector<uint8_t> data;
    };

    // ─── 统计计数器 ────────────────────────────────────────────────────────────
    struct Stats {
        std::atomic<uint64_t> recv_pkt{0};
        std::atomic<uint64_t> bad_pkt{0};
        std::atomic<uint64_t> complete_nalu{0};
        std::atomic<uint64_t> dropped_nalu{0};
        std::atomic<uint64_t> timeout_nalu{0};
        std::atomic<uint64_t> written_nalu{0};
    };
    Stats stats;

    std::unordered_map<uint32_t, FrameAssembler> pending;
    std::unordered_set<uint32_t> completed_ids;

    ThreadSafeBoundedQueue<std::vector<uint8_t>> buf_queue;
    ThreadSafeBoundedQueue<CompleteNALU> out_queue;
    SafeThread thread_, show_thread_;

    static bool parse_header(const uint8_t* buf, size_t len, PktHeader& out);

    static void fill_header(PktHeader& h,
                         uint8_t  pkt_type,
                         uint32_t frame_id,
                         uint16_t frag_index,
                         uint16_t frag_total,
//                         uint8_t  nalu_type,
                         uint16_t payload_len);


public:
    UdpSocket udp_socket_;

    struct in_addr udp_ip;
    in_port_t udp_port;

    void send_nalu(uint32_t frame_id, const NALU& nalu);

    void create(const std::string& ip, uint16_t port);

    // ─── 工具：获取当前时间（秒）─────────────────────────────────────────────
    static double now_sec();

    UdpNalu() : buf_queue(30), out_queue(30) {}
    virtual ~UdpNalu();
};


