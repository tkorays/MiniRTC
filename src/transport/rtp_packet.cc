/**
 * @file rtp_packet.cc
 * @brief MiniRTC RTP packet implementation - Optimized version
 */

#include "minirtc/transport/rtp_packet.h"

#include <cstring>
#include <ctime>
#include <algorithm>

namespace minirtc {

// ============================================================================
// RtpPacket Implementation
// ============================================================================

RtpPacket::RtpPacket() {
  buffer_.reserve(kMaxPacketSize);
}

RtpPacket::RtpPacket(uint8_t payload_type, uint32_t timestamp, uint16_t seq)
    : payload_type_(payload_type),
      timestamp_(timestamp),
      seq_(seq) {
  buffer_.reserve(kMaxPacketSize);
}

void RtpPacket::PreallocateBuffer() {
  buffer_.reserve(kMaxPacketSize);
  buffer_.resize(kMaxHeaderSize);
  payload_.reserve(kMaxPayloadSize);
  extension_data_.reserve(64);
  csrc_list_.reserve(15);
}

void RtpPacket::AddCsrc(uint32_t csrc) {
  if (csrc_list_.size() < 15) {
    csrc_list_.push_back(csrc);
    csrc_count_ = static_cast<uint8_t>(csrc_list_.size());
  }
}

int RtpPacket::SetPayload(const uint8_t* data, size_t size) {
  if (MINIRTC_UNLIKELY(size > kMaxPayloadSize)) {
    return -1;
  }
  payload_.assign(data, data + size);
  return 0;
}

int RtpPacket::SetPayload(const std::vector<uint8_t>& data) {
  return SetPayload(data.data(), data.size());
}

int RtpPacket::SetPayload(std::vector<uint8_t>&& data) noexcept {
  if (MINIRTC_UNLIKELY(data.size() > kMaxPayloadSize)) {
    return -1;
  }
  payload_ = std::move(data);
  return 0;
}

std::vector<uint8_t> RtpPacket::GetPayloadMove() {
  std::vector<uint8_t> result = std::move(payload_);
  return result;
}

int RtpPacket::SetExtensionData(const uint8_t* data, size_t size) {
  if (MINIRTC_UNLIKELY(size > 256)) {
    return -1;
  }
  extension_ = true;
  extension_data_.assign(data, data + size);
  return 0;
}

int RtpPacket::Serialize() {
  buffer_.clear();
  
  // Reserve space for header + payload
  size_t estimated_size = kMaxHeaderSize + payload_.size();
  buffer_.reserve(estimated_size);

  // Serialize header
  size_t header_size = SerializeHeader(buffer_.data());

  // Extension
  if (extension_ && !extension_data_.empty()) {
    buffer_.resize(header_size + 4);
    Write16(buffer_.data() + header_size, extension_profile_);
    Write16(buffer_.data() + header_size + 2,
            static_cast<uint16_t>(extension_data_.size() / 4));

    size_t padded_size = ((extension_data_.size() + 3) & ~3u);
    buffer_.insert(buffer_.end(), extension_data_.begin(),
                   extension_data_.end());
    if (padded_size > extension_data_.size()) {
      buffer_.insert(buffer_.end(),
                      padded_size - extension_data_.size(), 0);
    }
  }

  // Payload (use move semantics if possible)
  if (!payload_.empty()) {
    buffer_.insert(buffer_.end(), payload_.begin(), payload_.end());
  }

  return 0;
}

size_t RtpPacket::SerializeHeader(uint8_t* buffer) const {
  // First byte: V(2) P(1) X(1) CC(4)
  buffer[0] = (version_ << 6) | (padding_ << 5) |
              (extension_ << 4) | csrc_count_;

  // Second byte: M(1) PT(7)
  buffer[1] = (marker_ << 7) | payload_type_;

  // Sequence number (16 bits)
  Write16(buffer + 2, seq_);

  // Timestamp (32 bits)
  Write32(buffer + 4, timestamp_);

  // SSRC (32 bits)
  Write32(buffer + 8, ssrc_);

  // CSRC list
  size_t offset = 12;
  for (uint8_t i = 0; i < csrc_count_ && i < csrc_list_.size(); ++i) {
    Write32(buffer + offset, csrc_list_[i]);
    offset += 4;
  }

  return offset;
}

int RtpPacket::Deserialize(const uint8_t* data, size_t size) {
  if (MINIRTC_UNLIKELY(size < 12)) {
    return -1;
  }

  int result = DeserializeHeader(data, size);
  if (result < 0) {
    return result;
  }

  // Parse extension if present
  if (extension_) {
    size_t header_size = 12 + csrc_count_ * 4;
    if (MINIRTC_UNLIKELY(size < header_size + 4)) {
      return -1;
    }

    extension_profile_ = Read16(data + header_size);
    uint16_t ext_len = Read16(data + header_size + 2) * 4;

    size_t ext_offset = header_size + 4;
    if (MINIRTC_UNLIKELY(size < ext_offset + ext_len)) {
      return -1;
    }

    extension_data_.assign(data + ext_offset, data + ext_offset + ext_len);

    size_t payload_offset = ext_offset + ext_len;
    payload_.assign(data + payload_offset, data + size);
  } else {
    size_t header_size = 12 + csrc_count_ * 4;
    payload_.assign(data + header_size, data + size);
  }

  // Rebuild buffer (could be zero-copy if we kept the original reference)
  buffer_.assign(data, data + size);

  return 0;
}

int RtpPacket::BuildFromView(const uint8_t* data, size_t size) {
  // Zero-copy: just store reference instead of copying
  if (MINIRTC_UNLIKELY(size < 12)) {
    return -1;
  }

  int result = DeserializeHeader(data, size);
  if (result < 0) {
    return result;
  }

  // For zero-copy, we borrow the data - mark buffer as external
  // In this implementation we still copy for safety, but the interface allows optimization
  buffer_.assign(data, data + size);

  return 0;
}

int RtpPacket::DeserializeHeader(const uint8_t* data, size_t size) {
  if (MINIRTC_UNLIKELY(size < 12)) {
    return -1;
  }

  // First byte
  version_ = (data[0] >> 6) & 0x03;
  padding_ = (data[0] >> 5) & 0x01;
  extension_ = (data[0] >> 4) & 0x01;
  csrc_count_ = data[0] & 0x0F;

  // Second byte
  marker_ = (data[1] >> 7) & 0x01;
  payload_type_ = data[1] & 0x7F;

  // Validate version
  if (MINIRTC_UNLIKELY(version_ != 2)) {
    return -1;
  }

  // Check minimum size
  size_t min_size = 12 + csrc_count_ * 4;
  if (MINIRTC_UNLIKELY(size < min_size)) {
    return -1;
  }

  // Header fields (using fast read)
  seq_ = Read16(data + 2);
  timestamp_ = Read32(data + 4);
  ssrc_ = Read32(data + 8);

  // CSRC list
  csrc_list_.clear();
  csrc_list_.reserve(csrc_count_);
  for (uint8_t i = 0; i < csrc_count_; ++i) {
    csrc_list_.push_back(Read32(data + 12 + i * 4));
  }

  return 0;
}

std::shared_ptr<RtpPacket> RtpPacket::Clone() const {
  auto packet = std::make_shared<RtpPacket>();
  packet->version_ = version_;
  packet->padding_ = padding_;
  packet->extension_ = extension_;
  packet->csrc_count_ = csrc_count_;
  packet->marker_ = marker_;
  packet->payload_type_ = payload_type_;
  packet->seq_ = seq_;
  packet->timestamp_ = timestamp_;
  packet->ssrc_ = ssrc_;
  packet->csrc_list_ = csrc_list_;
  packet->extension_profile_ = extension_profile_;
  packet->extension_data_ = extension_data_;
  packet->payload_ = payload_;
  packet->Serialize();
  return packet;
}

void RtpPacket::Reset() {
  version_ = 2;
  padding_ = false;
  extension_ = false;
  csrc_count_ = 0;
  marker_ = 0;
  payload_type_ = 0;
  seq_ = 0;
  timestamp_ = 0;
  ssrc_ = 0;
  csrc_list_.clear();
  extension_profile_ = 0;
  extension_data_.clear();
  payload_.clear();
  buffer_.clear();
}

std::string RtpPacket::ToString() const {
  char buf[256];
  snprintf(buf, sizeof(buf),
           "RTP: PT=%u Seq=%u TS=%u SSRC=0x%08X Size=%zu",
           payload_type_, seq_, timestamp_, ssrc_, GetSize());
  return buf;
}

// ============================================================================
// Batch Serializer Implementation
// ============================================================================

size_t RtpPacketBatchSerializer::SerializeBatch(uint8_t* output_buffer, 
                                                  size_t buffer_capacity) {
  uint8_t* current = output_buffer;
  size_t remaining = buffer_capacity;

  for (const auto& packet : packets_) {
    size_t packet_size = packet->GetSize();
    if (packet_size > remaining) {
      break;  // Buffer full
    }

    // Copy packet data
    std::memcpy(current, packet->GetData(), packet_size);
    current += packet_size;
    remaining -= packet_size;
  }

  return current - output_buffer;
}

// ============================================================================
// Factory Functions
// ============================================================================

std::shared_ptr<RtpPacket> CreateRtpPacket() {
  auto packet = std::make_shared<RtpPacket>();
  packet->PreallocateBuffer();
  return packet;
}

std::shared_ptr<RtpPacket> CreateRtpPacket(uint8_t payload_type,
                                            uint32_t timestamp,
                                            uint16_t seq) {
  auto packet = std::make_shared<RtpPacket>(payload_type, timestamp, seq);
  packet->PreallocateBuffer();
  return packet;
}

}  // namespace minirtc
