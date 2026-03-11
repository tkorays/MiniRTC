#ifndef MINIRTC_H264_PACKER_H_
#define MINIRTC_H264_PACKER_H_

#include <memory>
#include <vector>
#include "minirtc/transport/rtp_packet.h"

namespace minirtc {

// H.264 NALU类型
enum class H264NaluType {
    kNonIFrame = 1,      // 非IDR Slice
    kIdrSlice = 5,        // IDR Slice
    kSei = 6,             // SEI
    kSps = 7,             // SPS
    kPps = 8,             // PPS
    kAud = 9,             // AUD
    kEndOfSeq = 10,       // End of sequence
    kEndOfStream = 11,    // End of stream
    kFua = 28,            // FU-A
    kFub = 29             // FU-B
};

// FU-A分片信息
struct FuAFragment {
    bool start;           // 是否是分片的开始
    bool end;             // 是否是分片的结束
    uint8_t nalu_header; // 原始NALU头
};

// IH264Packer 接口
class IH264Packer {
public:
    using Ptr = std::shared_ptr<IH264Packer>;
    virtual ~IH264Packer() = default;
    
    // 打包单个NALU（<= MTU）
    virtual std::vector<std::shared_ptr<RtpPacket>> PackNalu(
        const uint8_t* nalu_data,
        size_t nalu_size,
        uint32_t timestamp,
        bool marker
    ) = 0;
    
    // 打包FU-A分片（> MTU）
    virtual std::vector<std::shared_ptr<RtpPacket>> PackFuA(
        const uint8_t* nalu_data,
        size_t nalu_size,
        uint32_t timestamp,
        bool marker
    ) = 0;
    
    // 打包完整帧（自动选择Single NAL或FU-A）
    virtual std::vector<std::shared_ptr<RtpPacket>> PackFrame(
        const uint8_t* frame_data,
        size_t frame_size,
        uint32_t timestamp,
        bool marker,
        size_t max_packet_size = 1200
    ) = 0;
};

// IVideoAssembler 接口 - 组帧
class IVideoAssembler {
public:
    using Ptr = std::shared_ptr<IVideoAssembler>;
    virtual ~IVideoAssembler() = default;
    
    // 添加RTP包
    virtual void AddPacket(std::shared_ptr<RtpPacket> packet) = 0;
    
    // 获取完整帧（如果已组完）
    virtual std::shared_ptr<std::vector<uint8_t>> GetFrame() = 0;
    
    // 是否正在接收分片
    virtual bool IsInProgress() const = 0;
    
    // 重置
    virtual void Reset() = 0;
};

// H264Packer 实现
class H264Packer : public IH264Packer {
public:
    H264Packer();
    explicit H264Packer(uint8_t payload_type);
    ~H264Packer() override = default;

    // 设置payload type
    void SetPayloadType(uint8_t pt) { payload_type_ = pt; }
    uint8_t GetPayloadType() const { return payload_type_; }

    // 设置SSRC
    void SetSsrc(uint32_t ssrc) { ssrc_ = ssrc; }
    uint32_t GetSsrc() const { return ssrc_; }

    // 设置序列号
    void SetSequenceNumber(uint16_t seq) { seq_ = seq; }
    uint16_t GetSequenceNumber() const { return seq_; }

    // IH264Packer 实现
    std::vector<std::shared_ptr<RtpPacket>> PackNalu(
        const uint8_t* nalu_data,
        size_t nalu_size,
        uint32_t timestamp,
        bool marker
    ) override;

    std::vector<std::shared_ptr<RtpPacket>> PackFuA(
        const uint8_t* nalu_data,
        size_t nalu_size,
        uint32_t timestamp,
        bool marker
    ) override;

    std::vector<std::shared_ptr<RtpPacket>> PackFrame(
        const uint8_t* frame_data,
        size_t frame_size,
        uint32_t timestamp,
        bool marker,
        size_t max_packet_size = 1200
    ) override;

private:
    // 获取NALU类型
    H264NaluType GetNaluType(uint8_t nalu_header) const;

    // 判断是否是FU-A分片的开始
    bool IsFuAStart(uint8_t nalu_header) const;

    // 判断是否是FU-A分片的结束
    bool IsFuAEnd(uint8_t nalu_header, bool marker, bool is_last_fragment) const;

    uint8_t payload_type_ = 96;      // 默认H.264 payload type
    uint32_t ssrc_ = 0;              // SSRC
    uint16_t seq_ = 0;               // 序列号
};

// VideoAssembler 实现 - 组帧
class VideoAssembler : public IVideoAssembler {
public:
    VideoAssembler();
    ~VideoAssembler() override = default;

    // IVideoAssembler 实现
    void AddPacket(std::shared_ptr<RtpPacket> packet) override;
    std::shared_ptr<std::vector<uint8_t>> GetFrame() override;
    bool IsInProgress() const override;
    void Reset() override;

private:
    // 处理FU-A分片
    void HandleFuA(const uint8_t* payload, size_t payload_size);

    // 处理Single NAL Unit
    void HandleSingleNalu(const uint8_t* payload, size_t payload_size);

    // 检查是否接收完成
    bool IsComplete() const;

    std::shared_ptr<std::vector<uint8_t>> frame_data_;  // 组帧数据
    bool in_progress_ = false;                          // 是否正在接收分片
    uint8_t last_nalu_header_ = 0;                      // 最后一个NALU头
    uint32_t last_timestamp_ = 0;                       // 最后一个时间戳
    bool has_start_ = false;                            // 是否收到开始分片
};

// 创建H264Packer
std::shared_ptr<IH264Packer> CreateH264Packer();

// 创建VideoAssembler
std::shared_ptr<IVideoAssembler> CreateVideoAssembler();

}  // namespace minirtc

#endif  // MINIRTC_H264_PACKER_H_
