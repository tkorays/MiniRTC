/**
 * @file h264_packer.cc
 * @brief H.264 RTP packing and frame assembly implementation
 */

#include "minirtc/codec/h264_packer.h"

#include <cstring>
#include <algorithm>

namespace minirtc {

// ============================================================================
// H264Packer Implementation
// ============================================================================

H264Packer::H264Packer()
    : payload_type_(96), ssrc_(0), seq_(0) {
}

H264Packer::H264Packer(uint8_t payload_type)
    : payload_type_(payload_type), ssrc_(0), seq_(0) {
}

H264NaluType H264Packer::GetNaluType(uint8_t nalu_header) const {
    // NALU类型在第1个字节的低5位
    uint8_t nal_unit_type = nalu_header & 0x1F;
    
    switch (nal_unit_type) {
        case 1:  return H264NaluType::kNonIFrame;
        case 5:  return H264NaluType::kIdrSlice;
        case 6:  return H264NaluType::kSei;
        case 7:  return H264NaluType::kSps;
        case 8:  return H264NaluType::kPps;
        case 9:  return H264NaluType::kAud;
        case 10: return H264NaluType::kEndOfSeq;
        case 11: return H264NaluType::kEndOfStream;
        case 28: return H264NaluType::kFua;
        case 29: return H264NaluType::kFub;
        default: return H264NaluType::kNonIFrame;
    }
}

bool H264Packer::IsFuAStart(uint8_t nalu_header) const {
    // FU indicator的type为28(FU-A)或29(FU-B)
    // FU header的S位表示开始
    return (nalu_header & 0x1F) == 28;
}

bool H264Packer::IsFuAEnd(uint8_t nalu_header, bool marker, bool is_last_fragment) const {
    // FU header的E位表示结束，或者这是最后一个分片
    return is_last_fragment;
}

std::vector<std::shared_ptr<RtpPacket>> H264Packer::PackNalu(
    const uint8_t* nalu_data,
    size_t nalu_size,
    uint32_t timestamp,
    bool marker) {
    
    std::vector<std::shared_ptr<RtpPacket>> packets;
    
    // 创建RTP包
    auto packet = std::make_shared<RtpPacket>(payload_type_, timestamp, seq_++);
    packet->SetSsrc(ssrc_);
    packet->SetMarker(marker ? 1 : 0);
    
    // 设置payload为完整的NALU数据
    if (packet->SetPayload(nalu_data, nalu_size) != 0) {
        return packets;  // 返回空vector
    }
    
    // 序列化
    packet->Serialize();
    packets.push_back(packet);
    
    return packets;
}

std::vector<std::shared_ptr<RtpPacket>> H264Packer::PackFuA(
    const uint8_t* nalu_data,
    size_t nalu_size,
    uint32_t timestamp,
    bool marker) {
    
    std::vector<std::shared_ptr<RtpPacket>> packets;
    
    if (nalu_size < 2) {
        return packets;
    }
    
    // 原始NALU头
    uint8_t nalu_header = nalu_data[0];
    
    // NALU类型在低5位
    uint8_t nal_type = nalu_header & 0x1F;
    
    // FU indicator: F(1) | NRI(2) | Type(5) = 28 (FU-A)
    uint8_t fu_indicator = (nalu_header & 0xE0) | 28;
    
    // FU header: S | E | R | Type
    uint8_t fu_header = nal_type;
    
    // 有效负载起始位置（跳过原始NALU头）
    const uint8_t* payload_start = nalu_data + 1;
    size_t payload_size = nalu_size - 1;
    
    // 计算每个FU-A包的最大payload大小
    // RTP header(12) + FU indicator(1) + FU header(1) = 14
    // 假设MTU为1200，则最大payload为 1200 - 14 = 1186
    constexpr size_t kFuHeaderSize = 2;  // FU indicator + FU header
    size_t max_payload_size = 1186;
    
    // 分片发送
    size_t offset = 0;
    bool is_start = true;
    
    while (offset < payload_size) {
        size_t chunk_size = std::min(payload_size - offset, max_payload_size);
        bool is_end = (offset + chunk_size >= payload_size) || marker;
        
        // 创建RTP包
        auto packet = std::make_shared<RtpPacket>(payload_type_, timestamp, seq_++);
        packet->SetSsrc(ssrc_);
        
        // 设置FU indicator和FU header
        uint8_t fu_indicator_byte = fu_indicator;
        uint8_t fu_header_byte = fu_header;
        
        if (is_start) {
            fu_header_byte |= 0x80;  // S bit = 1
        }
        if (is_end) {
            fu_header_byte |= 0x40;  // E bit = 1
            packet->SetMarker(1);
        }
        
        // 构建payload: FU indicator + FU header + fragment
        std::vector<uint8_t> payload;
        payload.reserve(kFuHeaderSize + chunk_size);
        payload.push_back(fu_indicator_byte);
        payload.push_back(fu_header_byte);
        payload.insert(payload.end(), payload_start + offset, payload_start + offset + chunk_size);
        
        packet->SetPayload(payload.data(), payload.size());
        packet->Serialize();
        packets.push_back(packet);
        
        offset += chunk_size;
        is_start = false;
    }
    
    return packets;
}

std::vector<std::shared_ptr<RtpPacket>> H264Packer::PackFrame(
    const uint8_t* frame_data,
    size_t frame_size,
    uint32_t timestamp,
    bool marker,
    size_t max_packet_size) {
    
    std::vector<std::shared_ptr<RtpPacket>> packets;
    
    if (frame_data == nullptr || frame_size == 0) {
        return packets;
    }
    
    // 计算RTP头部大小
    constexpr size_t kRtpHeaderSize = 12;
    
    // 如果NALU <= max_packet_size，使用Single NAL Unit模式
    if (frame_size <= max_packet_size) {
        return PackNalu(frame_data, frame_size, timestamp, marker);
    }
    
    // 否则使用FU-A分片
    return PackFuA(frame_data, frame_size, timestamp, marker);
}

// ============================================================================
// VideoAssembler Implementation
// ============================================================================

VideoAssembler::VideoAssembler()
    : frame_data_(nullptr), in_progress_(false), last_nalu_header_(0),
      last_timestamp_(0), has_start_(false) {
}

void VideoAssembler::HandleFuA(const uint8_t* payload, size_t payload_size) {
    if (payload_size < 2) {
        return;
    }
    
    // 解析FU-A
    uint8_t fu_indicator = payload[0];
    uint8_t fu_header = payload[1];
    
    // 检查S位（开始）和E位（结束）
    bool start = (fu_header & 0x80) != 0;
    bool end = (fu_header & 0x40) != 0;
    
    // 原始NALU类型
    uint8_t nalu_type = fu_indicator & 0x1F;
    
    // 恢复原始NALU头
    uint8_t nalu_header = (fu_indicator & 0xE0) | (fu_header & 0x1F);
    
    if (start) {
        // 开始新的帧
        frame_data_ = std::make_shared<std::vector<uint8_t>>();
        frame_data_->push_back(nalu_header);
        has_start_ = true;
        in_progress_ = true;
    }
    
    if (has_start_ && frame_data_) {
        // 添加FU-A分片数据（跳过FU indicator和FU header）
        frame_data_->insert(frame_data_->end(), 
                           payload + 2, 
                           payload + payload_size);
    }
    
    if (end) {
        in_progress_ = false;
    }
}

void VideoAssembler::HandleSingleNalu(const uint8_t* payload, size_t payload_size) {
    // 完整的NALU，直接存储
    frame_data_ = std::make_shared<std::vector<uint8_t>>(payload, payload + payload_size);
    in_progress_ = false;
}

bool VideoAssembler::IsComplete() const {
    return !in_progress_ && frame_data_ && !frame_data_->empty();
}

void VideoAssembler::AddPacket(std::shared_ptr<RtpPacket> packet) {
    if (!packet) {
        return;
    }
    
    const uint8_t* payload = packet->GetPayload();
    size_t payload_size = packet->GetPayloadSize();
    uint32_t timestamp = packet->GetTimestamp();
    bool marker = packet->GetMarker() != 0;
    
    if (payload_size < 1) {
        return;
    }
    
    // 检查是否是FU-A
    uint8_t first_byte = payload[0];
    uint8_t nal_type = first_byte & 0x1F;
    
    // FU indicator: type = 28
    if (nal_type == 28) {
        HandleFuA(payload, payload_size);
    } else {
        // Single NAL Unit
        HandleSingleNalu(payload, payload_size);
    }
    
    last_timestamp_ = timestamp;
}

std::shared_ptr<std::vector<uint8_t>> VideoAssembler::GetFrame() {
    if (IsComplete()) {
        auto frame = frame_data_;
        // 重置状态
        frame_data_ = nullptr;
        has_start_ = false;
        return frame;
    }
    return nullptr;
}

bool VideoAssembler::IsInProgress() const {
    return in_progress_;
}

void VideoAssembler::Reset() {
    frame_data_ = nullptr;
    in_progress_ = false;
    has_start_ = false;
    last_nalu_header_ = 0;
    last_timestamp_ = 0;
}

// ============================================================================
// Factory Functions
// ============================================================================

std::shared_ptr<IH264Packer> CreateH264Packer() {
    return std::make_shared<H264Packer>();
}

std::shared_ptr<IVideoAssembler> CreateVideoAssembler() {
    return std::make_shared<VideoAssembler>();
}

}  // namespace minirtc
