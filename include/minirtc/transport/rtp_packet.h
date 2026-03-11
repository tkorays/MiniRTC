/**
 * @file rtp_packet.h
 * @brief MiniRTC RTP packet definition - Optimized version
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
// Compiler Hints
// ============================================================================

#if defined(__GNUC__) || defined(__clang__)
#define MINIRTC_LIKELY(x) __builtin_expect(!!(x), 1)
#define MINIRTC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define MINIRTC_INLINE inline __attribute__((always_inline))
#else
#define MINIRTC_LIKELY(x) (x)
#define MINIRTC_UNLIKELY(x) (x)
#define MINIRTC_INLINE inline
#endif

// ============================================================================
// RTP Packet
// ============================================================================

/// Optimized RTP packet implementation with zero-copy support
class RtpPacket {
 public:
  /// Maximum RTP header size (with extension)
  static constexpr size_t kMaxHeaderSize = 12 + 40;

  /// Maximum payload size (typical MTU - IP/UDP/RTP header)
  static constexpr size_t kMaxPayloadSize = 1460;

  /// Maximum packet size
  static constexpr size_t kMaxPacketSize = kMaxHeaderSize + kMaxPayloadSize;

  /// Cache line size for alignment
  static constexpr size_t kCacheLineSize = 64;

  /// Default constructor
  RtpPacket();

  /// Constructor with payload
  RtpPacket(uint8_t payload_type, uint32_t timestamp, uint16_t seq);

  /// Destructor
  virtual ~RtpPacket() = default;

  // ========================================================================
  // Pre-allocation for Zero-Copy
  // ========================================================================

  /// Pre-allocate buffers to avoid reallocation
  void PreallocateBuffer();

  // ========================================================================
  // RTP Header Fields (Inline for performance)
  // ========================================================================

  MINIRTC_INLINE uint8_t GetVersion() const { return version_; }
  MINIRTC_INLINE void SetVersion(uint8_t v) { version_ = v & 0x03; }

  MINIRTC_INLINE bool GetPadding() const { return padding_; }
  MINIRTC_INLINE void SetPadding(bool p) { padding_ = p; }

  MINIRTC_INLINE bool GetExtension() const { return extension_; }
  MINIRTC_INLINE void SetExtension(bool e) { extension_ = e; }

  MINIRTC_INLINE uint8_t GetCsrcCount() const { return csrc_count_; }
  MINIRTC_INLINE void SetCsrcCount(uint8_t count) { csrc_count_ = count & 0x0F; }

  MINIRTC_INLINE uint8_t GetMarker() const { return marker_; }
  MINIRTC_INLINE void SetMarker(uint8_t m) { marker_ = m & 0x01; }

  MINIRTC_INLINE uint8_t GetPayloadType() const { return payload_type_; }
  MINIRTC_INLINE void SetPayloadType(uint8_t pt) { payload_type_ = pt & 0x7F; }

  MINIRTC_INLINE uint16_t GetSequenceNumber() const { return seq_; }
  MINIRTC_INLINE void SetSequenceNumber(uint16_t seq) { seq_ = seq; }

  MINIRTC_INLINE uint32_t GetTimestamp() const { return timestamp_; }
  MINIRTC_INLINE void SetTimestamp(uint32_t ts) { timestamp_ = ts; }

  MINIRTC_INLINE uint32_t GetSsrc() const { return ssrc_; }
  MINIRTC_INLINE void SetSsrc(uint32_t s) { ssrc_ = s; }

  // ========================================================================
  // CSRC List
  // ========================================================================

  const std::vector<uint32_t>& GetCsrcList() const { return csrc_list_; }
  void AddCsrc(uint32_t csrc);
  void ClearCsrc() { 
    csrc_list_.clear(); 
    csrc_count_ = 0; 
  }

  // ========================================================================
  // Payload (Zero-copy support)
  // ========================================================================

  const uint8_t* GetPayload() const { return payload_.data(); }
  uint8_t* GetMutablePayload() { return payload_.data(); }
  size_t GetPayloadSize() const { return payload_.size(); }

  /// Set payload (uses move semantics when possible)
  int SetPayload(const uint8_t* data, size_t size);

  /// Set payload from vector (can use move)
  int SetPayload(const std::vector<uint8_t>& data);

  /// Set payload using move semantics (zero-copy)
  int SetPayload(std::vector<uint8_t>&& data) noexcept;

  /// Get payload as rvalue (for moving)
  std::vector<uint8_t> GetPayloadMove();

  void ClearPayload() { payload_.clear(); }

  // ========================================================================
  // Extension Header
  // ========================================================================

  bool HasExtension() const { return extension_; }

  uint16_t GetExtensionProfile() const { return extension_profile_; }
  void SetExtensionProfile(uint16_t profile) { extension_profile_ = profile; }

  const uint8_t* GetExtensionData() const { return extension_data_.data(); }
  size_t GetExtensionSize() const { return extension_data_.size(); }

  int SetExtensionData(const uint8_t* data, size_t size);

  void ClearExtension() {
    extension_ = false;
    extension_profile_ = 0;
    extension_data_.clear();
  }

  // ========================================================================
  // Packet Data (Zero-copy views)
  // ========================================================================

  const uint8_t* GetData() const { return buffer_.data(); }
  uint8_t* GetMutableData() { return buffer_.data(); }
  size_t GetSize() const { return buffer_.size(); }

  /// Serialize packet to buffer
  int Serialize();

  /// Deserialize packet from buffer
  int Deserialize(const uint8_t* data, size_t size);

  // ========================================================================
  // Zero-Copy View (for passing to network without copy)
  // ========================================================================

  /// Create a view of the packet data without ownership
  std::pair<const uint8_t*, size_t> GetView() const {
    return {buffer_.data(), buffer_.size()};
  }

  /// Build packet from view without copy (if external buffer)
  int BuildFromView(const uint8_t* data, size_t size);

  // ========================================================================
  // Utility
  // ========================================================================

  std::shared_ptr<RtpPacket> Clone() const;

  void Reset();

  std::string ToString() const;

 private:
  /// Serialize header to buffer
  size_t SerializeHeader(uint8_t* buffer) const;

  /// Deserialize header from buffer
  int DeserializeHeader(const uint8_t* data, size_t size);

  /// Fast network byte order operations (inline)
  MINIRTC_INLINE static uint16_t Read16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) << 8 | data[1];
  }

  MINIRTC_INLINE static uint32_t Read32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) << 24 |
           static_cast<uint32_t>(data[1]) << 16 |
           static_cast<uint32_t>(data[2]) << 8 |
           data[3];
  }

  MINIRTC_INLINE static void Write16(uint8_t* data, uint16_t value) {
    data[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[1] = static_cast<uint8_t>(value & 0xFF);
  }

  MINIRTC_INLINE static void Write32(uint8_t* data, uint32_t value) {
    data[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    data[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[3] = static_cast<uint8_t>(value & 0xFF);
  }

  // RTP header fields (optimized layout)
  uint8_t version_ = 2;
  bool padding_ = false;
  bool extension_ = false;
  uint8_t csrc_count_ = 0;
  uint8_t marker_ = 0;
  uint8_t payload_type_ = 0;

  uint16_t seq_ = 0;
  uint32_t timestamp_ = 0;
  uint32_t ssrc_ = 0;

  std::vector<uint32_t> csrc_list_;
  std::vector<uint8_t> extension_data_;
  uint16_t extension_profile_ = 0;

  std::vector<uint8_t> payload_;
  std::vector<uint8_t> buffer_;
};

// ============================================================================
// Batch Operations for Optimization
// ============================================================================

/// Batch serialize multiple packets (for batching)
class RtpPacketBatchSerializer {
 public:
  /// Add packet to batch
  void Add(std::shared_ptr<RtpPacket> packet) {
    if (packet && packet->GetSize() > 0) {
      packets_.push_back(packet);
    }
  }

  /// Serialize all packets to output buffer
  /// Returns total bytes written
  size_t SerializeBatch(uint8_t* output_buffer, size_t buffer_capacity);

  /// Get number of packets in batch
  size_t size() const { return packets_.size(); }

  /// Clear batch
  void Clear() { packets_.clear(); }

 private:
  std::vector<std::shared_ptr<RtpPacket>> packets_;
};

// ============================================================================
// RTX Packet (Retransmission)
// ============================================================================

struct RtxPacketInfo {
  uint16_t original_seq = 0;
  uint8_t payload_type = 0;
};

class RtpPacketWithRtx {
 public:
  RtpPacketWithRtx() = default;

  RtpPacket& GetPacket() { return packet_; }
  const RtpPacket& GetPacket() const { return packet_; }

  RtxPacketInfo& GetRtxInfo() { return rtx_info_; }
  const RtxPacketInfo& GetRtxInfo() const { return rtx_info_; }

  bool HasRtx() const { return rtx_info_.original_seq != 0; }

  void SetRtx(uint16_t original_seq, uint8_t payload_type) {
    rtx_info_.original_seq = original_seq;
    rtx_info_.payload_type = payload_type;
  }

  void ClearRtx() { rtx_info_ = {}; }

 private:
  RtpPacket packet_;
  RtxPacketInfo rtx_info_;
};

// ============================================================================
// Factory Functions
// ============================================================================

std::shared_ptr<RtpPacket> CreateRtpPacket();
std::shared_ptr<RtpPacket> CreateRtpPacket(uint8_t payload_type,
                                            uint32_t timestamp,
                                            uint16_t seq);

}  // namespace minirtc

#endif  // MINIRTC_RTP_PACKET_H
