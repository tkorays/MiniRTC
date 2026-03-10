/**
 * @file rtcp_packet.h
 * @brief MiniRTC RTCP packet definitions
 */

#ifndef MINIRTC_RTCP_PACKET_H
#define MINIRTC_RTCP_PACKET_H

#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <optional>

#include "transport_types.h"

namespace minirtc {

// ============================================================================
// RTCP Packet Base
// ============================================================================

/// RTCP packet base class
class RtcpPacket {
 public:
  /// Destructor
  virtual ~RtcpPacket() = default;

  /// Get packet type
  virtual RtcpPacketType GetType() const = 0;

  /// Get SSRC
  virtual uint32_t GetSsrc() const = 0;

  /// Get packet size (in bytes, must be 32-bit aligned)
  virtual size_t GetSize() const = 0;

  /// Serialize to buffer
  /// @param buffer Output buffer
  /// @param size Buffer size
  /// @return Serialized size, or negative on error
  virtual int Serialize(uint8_t* buffer, size_t size) const = 0;

  /// Deserialize from buffer
  /// @param data Input data
  /// @param size Input size
  /// @return 0 on success, negative on error
  virtual int Deserialize(const uint8_t* data, size_t size) = 0;

 protected:
  /// Common RTCP header size
  static constexpr size_t kRtcpHeaderSize = 4;

  /// Read network order 16-bit
  static uint16_t Read16(const uint8_t* data) {
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
  }

  /// Read network order 32-bit
  static uint32_t Read32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           data[3];
  }

  /// Write network order 16-bit
  static void Write16(uint8_t* data, uint16_t value) {
    data[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[1] = static_cast<uint8_t>(value & 0xFF);
  }

  /// Write network order 32-bit
  static void Write32(uint8_t* data, uint32_t value) {
    data[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    data[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[3] = static_cast<uint8_t>(value & 0xFF);
  }
};

// ============================================================================
// Sender Report (SR)
// ============================================================================

/// RTCP Sender Report packet
class RtcpSrPacket : public RtcpPacket {
 public:
  /// Default constructor
  RtcpSrPacket();

  /// Constructor with SSRC
  explicit RtcpSrPacket(uint32_t ssrc);

  /// Destructor
  ~RtcpSrPacket() override = default;

  /// Get packet type
  RtcpPacketType GetType() const override { return RtcpPacketType::kSR; }

  /// Get SSRC
  uint32_t GetSsrc() const override { return sender_ssrc_; }
  /// Set sender SSRC
  void SetSsrc(uint32_t ssrc) { sender_ssrc_ = ssrc; }

  /// Get NTP timestamp (high 32 bits)
  uint32_t GetNtpTimestampHigh() const { return ntp_timestamp_high_; }
  /// Get NTP timestamp (low 32 bits)
  uint32_t GetNtpTimestampLow() const { return ntp_timestamp_low_; }
  /// Set NTP timestamp
  void SetNtpTimestamp(uint32_t high, uint32_t low) {
    ntp_timestamp_high_ = high;
    ntp_timestamp_low_ = low;
  }

  /// Get RTP timestamp
  uint32_t GetRtpTimestamp() const { return rtp_timestamp_; }
  /// Set RTP timestamp
  void SetRtpTimestamp(uint32_t ts) { rtp_timestamp_ = ts; }

  /// Get sender's packet count
  uint32_t GetPacketCount() const { return sender_packet_count_; }
  /// Set sender's packet count
  void SetPacketCount(uint32_t count) { sender_packet_count_ = count; }

  /// Get sender's octet count
  uint32_t GetOctetCount() const { return sender_octet_count_; }
  /// Set sender's octet count
  void SetOctetCount(uint32_t count) { sender_octet_count_ = count; }

  /// Get report blocks
  const std::vector<RtcpReportBlock>& GetReportBlocks() const { return report_blocks_; }
  /// Add report block
  void AddReportBlock(const RtcpReportBlock& block);
  /// Clear report blocks
  void ClearReportBlocks() { report_blocks_.clear(); }

  /// Get packet size
  size_t GetSize() const override;

  /// Serialize
  int Serialize(uint8_t* buffer, size_t size) const override;

  /// Deserialize
  int Deserialize(const uint8_t* data, size_t size) override;

  /// Set NTP timestamp from current time
  void SetNtpTimestampNow();

  /// Get estimated timestamp (in microseconds since epoch)
  uint64_t GetNtpTimestampUs() const;

 private:
  uint32_t sender_ssrc_ = 0;             ///< Sender SSRC
  uint32_t ntp_timestamp_high_ = 0;      ///< NTP timestamp high
  uint32_t ntp_timestamp_low_ = 0;       ///< NTP timestamp low
  uint32_t rtp_timestamp_ = 0;            ///< RTP timestamp
  uint32_t sender_packet_count_ = 0;     ///< Sender packet count
  uint32_t sender_octet_count_ = 0;       ///< Sender octet count
  std::vector<RtcpReportBlock> report_blocks_;  ///< Report blocks
};

// ============================================================================
// Receiver Report (RR)
// ============================================================================

/// RTCP Receiver Report packet
class RtcpRrPacket : public RtcpPacket {
 public:
  /// Default constructor
  RtcpRrPacket();

  /// Constructor with SSRC
  explicit RtcpRrPacket(uint32_t ssrc);

  /// Destructor
  ~RtcpRrPacket() override = default;

  /// Get packet type
  RtcpPacketType GetType() const override { return RtcpPacketType::kRR; }

  /// Get SSRC
  uint32_t GetSsrc() const override { return sender_ssrc_; }
  /// Set sender SSRC
  void SetSsrc(uint32_t ssrc) { sender_ssrc_ = ssrc; }

  /// Get report blocks
  const std::vector<RtcpReportBlock>& GetReportBlocks() const { return report_blocks_; }
  /// Add report block
  void AddReportBlock(const RtcpReportBlock& block);
  /// Clear report blocks
  void ClearReportBlocks() { report_blocks_.clear(); }

  /// Get packet size
  size_t GetSize() const override;

  /// Serialize
  int Serialize(uint8_t* buffer, size_t size) const override;

  /// Deserialize
  int Deserialize(const uint8_t* data, size_t size) override;

 private:
  uint32_t sender_ssrc_ = 0;             ///< Sender SSRC
  std::vector<RtcpReportBlock> report_blocks_;  ///< Report blocks
};

// ============================================================================
// Source Description (SDES)
// ============================================================================

/// SDES item types
enum class SdesItemType {
  kEnd = 0,
  kCNAME = 1,
  kName = 2,
  kEmail = 3,
  kPhone = 4,
  kLoc = 5,
  kTool = 6,
  kNote = 7,
  kPriv = 8,
};

/// SDES item
struct SdesItem {
  SdesItemType type;
  std::string content;
};

/// RTCP Source Description packet
class RtcpSdesPacket : public RtcpPacket {
 public:
  /// Default constructor
  RtcpSdesPacket();

  /// Destructor
  ~RtcpSdesPacket() override = default;

  /// Get packet type
  RtcpPacketType GetType() const override { return RtcpPacketType::kSDES; }

  /// Get SSRC
  uint32_t GetSsrc() const override {
    return chunks_.empty() ? 0 : chunks_.front().ssrc;
  }

  /// Add SDES chunk
  struct SdesChunk {
    uint32_t ssrc;
    std::vector<SdesItem> items;
  };

  void AddChunk(const SdesChunk& chunk);
  void ClearChunks() { chunks_.clear(); }

  /// Get packet size
  size_t GetSize() const override;

  /// Serialize
  int Serialize(uint8_t* buffer, size_t size) const override;

  /// Deserialize
  int Deserialize(const uint8_t* data, size_t size) override;

  /// Add CNAME item
  void SetCname(uint32_t ssrc, const std::string& cname);

  /// Add NAME item
  void SetName(uint32_t ssrc, const std::string& name);

 private:
  std::vector<SdesChunk> chunks_;
};

// ============================================================================
// BYE Packet
// ============================================================================

/// RTCP BYE packet
class RtcpByePacket : public RtcpPacket {
 public:
  /// Default constructor
  RtcpByePacket();

  /// Destructor
  ~RtcpByePacket() override = default;

  /// Get packet type
  RtcpPacketType GetType() const override { return RtcpPacketType::kBYE; }

  /// Get SSRC
  uint32_t GetSsrc() const override {
    return ssrcs_.empty() ? 0 : ssrcs_.front();
  }

  /// Add SSRC
  void AddSsrc(uint32_t ssrc) { ssrcs_.push_back(ssrc); }

  /// Get SSRC list
  const std::vector<uint32_t>& GetSsrcs() const { return ssrcs_; }

  /// Set reason
  void SetReason(const std::string& reason);

  /// Get reason
  const std::string& GetReason() const { return reason_; }

  /// Get packet size
  size_t GetSize() const override;

  /// Serialize
  int Serialize(uint8_t* buffer, size_t size) const override;

  /// Deserialize
  int Deserialize(const uint8_t* data, size_t size) override;

 private:
  std::vector<uint32_t> ssrcs_;
  std::string reason_;
};

// ============================================================================
// NACK Packet (Feedback)
// ============================================================================

/// RTCP NACK packet (RTPFB)
class RtcpNackPacket : public RtcpPacket {
 public:
  /// Default constructor
  RtcpNackPacket();

  /// Constructor with SSRC
  RtcpNackPacket(uint32_t sender_ssrc, uint32_t media_ssrc);

  /// Destructor
  ~RtcpNackPacket() override = default;

  /// Get packet type
  RtcpPacketType GetType() const override { return RtcpPacketType::kRTPFB; }

  /// Get sender SSRC
  uint32_t GetSsrc() const override { return sender_ssrc_; }
  /// Set sender SSRC
  void SetSsrc(uint32_t ssrc) { sender_ssrc_ = ssrc; }

  /// Get media SSRC
  uint32_t GetMediaSsrc() const { return media_ssrc_; }
  /// Set media SSRC
  void SetMediaSsrc(uint32_t ssrc) { media_ssrc_ = ssrc; }

  /// Get NACK list
  const std::vector<uint16_t>& GetNackList() const { return nack_list_; }

  /// Add NACK sequence number
  void AddNack(uint16_t seq);

  /// Add NACK sequence number range
  void AddNackRange(uint16_t start, uint16_t end);

  /// Clear NACK list
  void ClearNackList() { nack_list_.clear(); }

  /// Get packet size
  size_t GetSize() const override;

  /// Serialize
  int Serialize(uint8_t* buffer, size_t size) const override;

  /// Deserialize
  int Deserialize(const uint8_t* data, size_t size) override;

 private:
  uint32_t sender_ssrc_ = 0;
  uint32_t media_ssrc_ = 0;
  std::vector<uint16_t> nack_list_;
};

// ============================================================================
// RTCP Compound Packet
// ============================================================================

/// RTCP compound packet
class RtcpCompoundPacket {
 public:
  /// Default constructor
  RtcpCompoundPacket() = default;

  /// Destructor
  ~RtcpCompoundPacket() = default;

  /// Add RTCP packet
  void AddPacket(std::shared_ptr<RtcpPacket> packet);

  /// Clear all packets
  void Clear() { packets_.clear(); }

  /// Get packets
  const std::vector<std::shared_ptr<RtcpPacket>>& GetPackets() const {
    return packets_;
  }

  /// Get total size
  size_t GetSize() const;

  /// Serialize all packets
  int Serialize(uint8_t* buffer, size_t size) const;

  /// Deserialize compound packet
  int Deserialize(const uint8_t* data, size_t size);

  /// Check if empty
  bool IsEmpty() const { return packets_.empty(); }

  /// Get packet count
  size_t GetPacketCount() const { return packets_.size(); }

 private:
  std::vector<std::shared_ptr<RtcpPacket>> packets_;
};

// ============================================================================
// Factory Functions
// ============================================================================

/// Create Sender Report
std::shared_ptr<RtcpSrPacket> CreateRtcpSr(uint32_t ssrc);

/// Create Receiver Report
std::shared_ptr<RtcpRrPacket> CreateRtcpRr(uint32_t ssrc);

/// Create SDES packet
std::shared_ptr<RtcpSdesPacket> CreateRtcpSdes();

/// Create BYE packet
std::shared_ptr<RtcpByePacket> CreateRtcpBye(const std::vector<uint32_t>& ssrcs,
                                              const std::string& reason = "");

/// Create NACK packet
std::shared_ptr<RtcpNackPacket> CreateRtcpNack(uint32_t sender_ssrc,
                                                 uint32_t media_ssrc,
                                                 const std::vector<uint16_t>& nacks);

/// Create compound packet
std::shared_ptr<RtcpCompoundPacket> CreateRtcpCompound();

}  // namespace minirtc

#endif  // MINIRTC_RTCP_PACKET_H
