/**
 * @file rtp_packet.h
 * @brief MiniRTC RTP packet definition
 */

#ifndef MINIRTC_RTP_PACKET_H
#define MINIRTC_RTP_PACKET_H

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <string>
#include <optional>

namespace minirtc {

// ============================================================================
// RTP Packet
// ============================================================================

/// RTP packet implementation
class RtpPacket {
 public:
  /// Maximum RTP header size (with extension)
  static constexpr size_t kMaxHeaderSize = 12 + 40;  // Basic + extension

  /// Maximum payload size
  static constexpr size_t kMaxPayloadSize = 1460;  // Typical MTU - IP/UDP/RTP header

  /// Maximum packet size
  static constexpr size_t kMaxPacketSize = kMaxHeaderSize + kMaxPayloadSize;

  /// Default constructor
  RtpPacket();

  /// Constructor with payload
  RtpPacket(uint8_t payload_type, uint32_t timestamp, uint16_t seq);

  /// Destructor
  virtual ~RtpPacket() = default;

  // ========================================================================
  // RTP Header Fields
  // ========================================================================

  /// Get version
  uint8_t GetVersion() const { return version_; }
  /// Set version
  void SetVersion(uint8_t v) { version_ = v & 0x03; }

  /// Get padding flag
  bool GetPadding() const { return padding_; }
  /// Set padding flag
  void SetPadding(bool p) { padding_ = p; }

  /// Get extension flag
  bool GetExtension() const { return extension_; }
  /// Set extension flag
  void SetExtension(bool e) { extension_ = e; }

  /// Get CSRC count
  uint8_t GetCsrcCount() const { return csrc_count_; }
  /// Set CSRC count
  void SetCsrcCount(uint8_t count) { csrc_count_ = count & 0x0F; }

  /// Get marker
  uint8_t GetMarker() const { return marker_; }
  /// Set marker
  void SetMarker(uint8_t m) { marker_ = m & 0x01; }

  /// Get payload type
  uint8_t GetPayloadType() const { return payload_type_; }
  /// Set payload type
  void SetPayloadType(uint8_t pt) { payload_type_ = pt & 0x7F; }

  /// Get sequence number
  uint16_t GetSequenceNumber() const { return seq_; }
  /// Set sequence number
  void SetSequenceNumber(uint16_t seq) { seq_ = seq; }

  /// Get timestamp
  uint32_t GetTimestamp() const { return timestamp_; }
  /// Set timestamp
  void SetTimestamp(uint32_t ts) { timestamp_ = ts; }

  /// Get SSRC
  uint32_t GetSsrc() const { return ssrc_; }
  /// Set SSRC
  void SetSsrc(uint32_t s) { ssrc_ = s; }

  // ========================================================================
  // CSRC List
  // ========================================================================

  /// Get CSRC list
  const std::vector<uint32_t>& GetCsrcList() const { return csrc_list_; }
  /// Add CSRC
  void AddCsrc(uint32_t csrc);
  /// Clear CSRC list
  void ClearCsrc() { csrc_list_.clear(); csrc_count_ = 0; }

  // ========================================================================
  // Payload
  // ========================================================================

  /// Get payload data
  const uint8_t* GetPayload() const { return payload_.data(); }
  /// Get payload size
  size_t GetPayloadSize() const { return payload_.size(); }

  /// Set payload (copies data)
  int SetPayload(const uint8_t* data, size_t size);

  /// Set payload from vector
  int SetPayload(const std::vector<uint8_t>& data);

  /// Clear payload
  void ClearPayload() { payload_.clear(); }

  // ========================================================================
  // Extension Header
  // ========================================================================

  /// Check if has extension
  bool HasExtension() const { return extension_; }

  /// Get extension profile
  uint16_t GetExtensionProfile() const { return extension_profile_; }
  /// Set extension profile
  void SetExtensionProfile(uint16_t profile) { extension_profile_ = profile; }

  /// Get extension data
  const uint8_t* GetExtensionData() const { return extension_data_.data(); }
  /// Get extension data size
  size_t GetExtensionSize() const { return extension_data_.size(); }

  /// Set extension data
  int SetExtensionData(const uint8_t* data, size_t size);

  /// Clear extension
  void ClearExtension() {
    extension_ = false;
    extension_profile_ = 0;
    extension_data_.clear();
  }

  // ========================================================================
  // Packet Data
  // ========================================================================

  /// Get complete packet data
  const uint8_t* GetData() const { return buffer_.data(); }

  /// Get packet size
  size_t GetSize() const { return buffer_.size(); }

  /// Serialize packet to buffer
  /// @return Serialized size, or negative on error
  int Serialize();

  /// Deserialize packet from buffer
  /// @return 0 on success, negative on error
  int Deserialize(const uint8_t* data, size_t size);

  // ========================================================================
  // Utility
  // ========================================================================

  /// Clone packet
  std::shared_ptr<RtpPacket> Clone() const;

  /// Reset packet
  void Reset();

  /// Debug info
  std::string ToString() const;

 private:
  /// Serialize header
  size_t SerializeHeader(uint8_t* buffer) const;

  /// Deserialize header
  int DeserializeHeader(const uint8_t* data, size_t size);

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

  // RTP header fields (network byte order for serialization)
  uint8_t version_ = 2;           ///< Version (V)
  bool padding_ = false;         ///< Padding (P)
  bool extension_ = false;       ///< Extension (X)
  uint8_t csrc_count_ = 0;      ///< CSRC count (CC)
  uint8_t marker_ = 0;           ///< Marker (M)
  uint8_t payload_type_ = 0;    ///< Payload type (PT)

  uint16_t seq_ = 0;             ///< Sequence number
  uint32_t timestamp_ = 0;      ///< Timestamp
  uint32_t ssrc_ = 0;           ///< SSRC

  std::vector<uint32_t> csrc_list_;  ///< CSRC list
  std::vector<uint8_t> extension_data_;  ///< Extension data

  uint16_t extension_profile_ = 0;  ///< Extension profile

  std::vector<uint8_t> payload_;  ///< Payload data
  std::vector<uint8_t> buffer_;   ///< Serialized buffer
};

// ============================================================================
// RTX Packet (Retransmission)
// ============================================================================

/// RTX packet information
struct RtxPacketInfo {
  uint16_t original_seq = 0;      ///< Original sequence number
  uint8_t payload_type = 0;      ///< RTX payload type
};

/// RTP packet with RTX support
class RtpPacketWithRtx {
 public:
  RtpPacketWithRtx() = default;

  /// Get RTP packet
  RtpPacket& GetPacket() { return packet_; }
  const RtpPacket& GetPacket() const { return packet_; }

  /// Get RTX info
  RtxPacketInfo& GetRtxInfo() { return rtx_info_; }
  const RtxPacketInfo& GetRtxInfo() const { return rtx_info_; }

  /// Has RTX info
  bool HasRtx() const { return rtx_info_.original_seq != 0; }

  /// Set RTX information
  void SetRtx(uint16_t original_seq, uint8_t payload_type) {
    rtx_info_.original_seq = original_seq;
    rtx_info_.payload_type = payload_type;
  }

  /// Clear RTX info
  void ClearRtx() { rtx_info_ = {}; }

 private:
  RtpPacket packet_;
  RtxPacketInfo rtx_info_;
};

// ============================================================================
// Factory Functions
// ============================================================================

/// Create RTP packet
std::shared_ptr<RtpPacket> CreateRtpPacket();

/// Create RTP packet with parameters
std::shared_ptr<RtpPacket> CreateRtpPacket(uint8_t payload_type,
                                            uint32_t timestamp,
                                            uint16_t seq);

}  // namespace minirtc

#endif  // MINIRTC_RTP_PACKET_H
