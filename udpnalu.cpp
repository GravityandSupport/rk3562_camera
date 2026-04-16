#include "udpnalu.h"

#include "outLog.h"

// ─── 协议常量 ──────────────────────────────────────────────────────────────
static constexpr uint8_t  MAGIC_0        = 0x55;
static constexpr uint8_t  MAGIC_1        = 0xAA;
static constexpr uint8_t  PROTO_VERSION  = 1;
static constexpr uint8_t  PKT_TYPE_DATA  = 0x01;
static constexpr uint8_t  PKT_TYPE_END   = 0x02; // 结束包
static constexpr uint16_t MAX_UDP_PAYLOAD = 1400;   // 避免IP分片

// ─── 协议 包头（大端序，紧凑布局） ──────────────────────────────────────────────────────────────
/*
magic(2) | 帧头
version(1) | 版本信息
frame_id(4) | 帧序号
pkt_type(1) | 当前包的类型，是数据包还是结束包
frag_index(2) | 当前包是 nalu 分包的第几个数据
frag_total(2) | nalu分包的总数
nalu_type(1) | nalu包类型
payload_len(2) | 数据长度
*/

#pragma pack(push, 1)
struct PktHeader {
    uint8_t  magic[2];
    uint8_t  version;
    uint8_t  pkt_type;
    uint32_t frame_id;
    uint16_t frag_index;    // 网络序
    uint16_t frag_total;    // 网络序
//    uint8_t  nalu_type;
    uint16_t payload_len;   // 网络序
};
#pragma pack(pop)

static constexpr size_t HEADER_SIZE   = sizeof(PktHeader);   // 11 字节   协议包头字节数
static constexpr size_t MAX_FRAG_SIZE = MAX_UDP_PAYLOAD - HEADER_SIZE; // 一次性可发送的naul包的字节数

void UdpNalu::fill_header(PktHeader& h,
                         uint8_t  pkt_type,
                         uint32_t frame_id,
                         uint16_t frag_index,
                         uint16_t frag_total,
//                         uint8_t  nalu_type,
                         uint16_t payload_len)
{
    h.magic[0]    = MAGIC_0;
    h.magic[1]    = MAGIC_1;
    h.version     = PROTO_VERSION;
    h.pkt_type    = pkt_type;
    h.frame_id    = htonl(frame_id);
    h.frag_index  = htons(frag_index);
    h.frag_total  = htons(frag_total);
//    h.nalu_type   = nalu_type;
    h.payload_len = htons(payload_len);
}

void UdpNalu::send_nalu(uint32_t frame_id, const NALU& nalu){
    LOG_DEBUG("udp nalu", inet_ntoa(udp_ip), ntohs(udp_port), nalu.size);
    uint16_t frag_total  = static_cast<uint16_t>(
        (nalu.size + MAX_FRAG_SIZE - 1) / MAX_FRAG_SIZE); // 计算nalu总共要分多少个包，向上取整

    std::vector<uint8_t> pkt(HEADER_SIZE + MAX_FRAG_SIZE);

    for (uint16_t idx = 0; idx < frag_total; ++idx){
        size_t   offset  = idx * MAX_FRAG_SIZE;
        uint16_t frag_sz = static_cast<uint16_t>(
            std::min(MAX_FRAG_SIZE, nalu.size - offset)); // 这次要发送的字节数，这是不含头部信息的字节数

        PktHeader hdr;
        fill_header(hdr, PKT_TYPE_DATA, frame_id, idx, frag_total, frag_sz);

        std::memcpy(pkt.data(), &hdr, HEADER_SIZE);
        std::memcpy(pkt.data() + HEADER_SIZE, nalu.data + offset, frag_sz);

        // 调用发送api
        UdpSocket::getInstance().sendTo(udp_ip, udp_port, pkt.data(), HEADER_SIZE + frag_sz);
    }

    // 帧结束标记
    PktHeader end_hdr;
    fill_header(end_hdr, PKT_TYPE_END, frame_id, 0, 0, 0);
    // 调用发送api
    UdpSocket::getInstance().sendTo(udp_ip, udp_port, &end_hdr, HEADER_SIZE);
}
UdpNalu::~UdpNalu(){

}

//////////////////////////////////////////////////////////////////

// ─── 工具：获取当前时间（秒）─────────────────────────────────────────────
double UdpNalu::now_sec() {
    using namespace std::chrono;
    return duration_cast<duration<double>>(
        steady_clock::now().time_since_epoch()).count();
}

static constexpr double REORDER_TIMEOUT  = 0.5;   // 秒
static constexpr size_t MAX_PENDING      = 64;

bool UdpNalu::FrameAssembler::is_complete() const {
    return frags.size() == static_cast<size_t>(frag_total);
}

bool UdpNalu::FrameAssembler::is_expired(double now) const {
    return (now - created_at) > REORDER_TIMEOUT;
}

// 按序拼接所有分片
std::vector<uint8_t> UdpNalu::FrameAssembler::assemble() const {
    std::vector<uint8_t> out;
    for (const auto& kv : frags) {
        out.insert(out.end(), kv.second.begin(), kv.second.end());
    }
    return out;
}

// ─── 解析包头 ─────────────────────────────────────────────────────────────
bool UdpNalu::parse_header(const uint8_t* buf, size_t len, PktHeader& out)
{
    if (len < HEADER_SIZE) return false;
    std::memcpy(&out, buf, HEADER_SIZE);
    if (out.magic[0] != MAGIC_0 || out.magic[1] != MAGIC_1) return false;
    if (out.version  != PROTO_VERSION) return false;
    // 转换网络序
    out.frame_id    = ntohl(out.frame_id);
    out.frag_index  = ntohs(out.frag_index);
    out.frag_total  = ntohs(out.frag_total);
    out.payload_len = ntohs(out.payload_len);
    return true;
}


void UdpNalu::create(const std::string& ip, uint16_t port){
    udp_socket_.create();
    udp_socket_.bind(ip, port);
    udp_socket_.start(2000);
    udp_socket_.registerCallback(ip, port, [&](const char* data, size_t len,
                                  const std::string& sender_ip,
                                      uint16_t sender_port,
                                  EpollEvent::Message message){
        if(message==EpollEvent::Message::Data){
            (void)sender_ip, (void)sender_port;
            std::vector<uint8_t> buf(data, data+len);
            ++stats.recv_pkt;
        }else if(message==EpollEvent::Message::Timeout){
            // 超时清理
            double now = now_sec();
            for (auto it = pending.begin(); it != pending.end(); ) {
                if (it->second.is_expired(now)) {
                    ++stats.timeout_nalu;
                    it = pending.erase(it);
                } else {
                    ++it;
                }
            }
        }
    });

    thread_.set_loop_callback([&](SafeThread* self)->bool{
        (void)self;
        std::vector<uint8_t> buf;
        if(buf_queue.pop(buf)){
            PktHeader hdr;
            if (!parse_header(buf.data(), buf.size(), hdr)) {
                ++stats.bad_pkt;
                return true;
            }

            double now = now_sec();

            // ── 帧结束标记 ────────────────────────────────────────────────────
            if (hdr.pkt_type == PKT_TYPE_END) {
                auto it = pending.find(hdr.frame_id);
                if (it != pending.end()) {
                    if (it->second.is_complete()) {
                        CompleteNALU cn;
                        cn.frame_id  = hdr.frame_id;
//                        cn.nalu_type = it->second.nalu_type;
                        cn.data      = it->second.assemble();
                        out_queue.push(std::move(cn));
                        ++stats.complete_nalu;
                    }
                    completed_ids.insert(hdr.frame_id);
                    pending.erase(it);
                }
                return true;
            }

            // ── 数据包 ────────────────────────────────────────────────────────
            if (hdr.pkt_type != PKT_TYPE_DATA) return true;
            if (completed_ids.count(hdr.frame_id)) return true;  // 重复包

            // 检查 payload 长度合法
            size_t total_size = HEADER_SIZE + hdr.payload_len;
            if (static_cast<size_t>(buf.size()) < total_size || hdr.payload_len == 0) {
                ++stats.bad_pkt;
                return true;
            }

            if (!pending.count(hdr.frame_id)) {
                // 驱逐最旧帧（防止内存溢出）
                if (pending.size() >= MAX_PENDING) {
                    uint32_t oldest = pending.begin()->first;
                    for (auto& kv : pending)
                        if (kv.first < oldest) oldest = kv.first;
                    pending.erase(oldest);
                    ++stats.dropped_nalu;
                }
                pending.emplace(hdr.frame_id,
                    FrameAssembler(hdr.frame_id, hdr.frag_total, /*hdr.nalu_type,*/ now));
            }

            auto& assembler = pending[hdr.frame_id];
            std::vector<uint8_t> payload(buf.data() + HEADER_SIZE,
                                         buf.data() + HEADER_SIZE + hdr.payload_len);
            assembler.frags[hdr.frag_index] = std::move(payload);

            // 超时清理
            for (auto it = pending.begin(); it != pending.end(); ) {
                if (it->second.is_expired(now)) {
                    ++stats.timeout_nalu;
                    it = pending.erase(it);
                } else {
                    ++it;
                }
            }
        }
        return true;
    });
    thread_.start();

    show_thread_.set_loop_callback([&](SafeThread* self)->bool{
        (void)self;
        CompleteNALU cn;
        if (!out_queue.pop(cn)) return true;

        ++stats.written_nalu;
        return  true;
    });
    show_thread_.start();
}








