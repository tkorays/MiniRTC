/**
 * @file rtp_packet.cc
 * @brief MiniRTC RTP packet implementation
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

void RtpPacket::AddCsrc(uint32_t csrc) {
  if (csrc_list_.size() < 15) {
    csrc_list_.push_back(csrc);
    csrc_count_ = static_cast<uint8_t>(csrc_list_.size());
  }
}

int RtpPacket::SetPayload(const uint8_t* data, size_t size) {
  if (size > kMaxPayloadSize) {
    return -1;  // Too large
  }
  payload_.assign(data, data + size);
  return 0;
}

int RtpPacket::SetPayload(const std::vector<uint8_t>& data) {
  return SetPayload(data.data(), data.size());
}

int RtpPacket::SetExtensionData(const uint8_t* data, size_t size) {
  if (size > 256) {  // Extension size limit
    return -1;
  }
  extension_ = true;
  extension_data_.assign(data, data + size);
  return 0;
}

int RtpPacket::Serialize() {
  buffer_.clear();
  buffer_.reserve(kMaxPacketSize);

  // Header
  size_t header_size = SerializeHeader(buffer_.data());

  // Extension
  if (extension_ && !extension_data_.empty()) {
    // Extension header (4 bytes)
    buffer_.resize(header_size + 4);
    Write16(buffer_.data() + header_size, extension_profile_);
    Write16(buffer_.data() + header_size + 2,
            static_cast<uint16_t>(extension_data_.size() / 4));

    // Extension data (must be padded to 4-byte boundary)
    size_t padded_size = ((extension_data_.size() + 3) & ~3u);
    buffer_.insert(buffer_.end(), extension_data_.begin(),
                   extension_data_.end());
    if (padded_size > extension_data_.size()) {
      buffer_.insert(buffer_.end(),
                      padded_size - extension_data_.size(), 0);
    }
  }

  // Payload
  buffer_.insert(buffer_.end(), payload_.begin(), payload_.end());

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
  if (size < 12) {
    return -1;  // Too small
  }

  int result = DeserializeHeader(data, size);
  if (result < 0) {
    return result;
  }

  // Parse extension if present
  if (extension_) {
    size_t header_size = 12 + csrc_count_ * 4;
    if (size < header_size + 4) {
      return -1;
    }

    extension_profile_ = Read16(data + header_size);
    uint16_t ext_len = Read16(data + header_size + 2) * 4;

    size_t ext_offset = header_size + 4;
    if (size < ext_offset + ext_len) {
      return -1;
    }

    extension_data_.assign(data + ext_offset, data + ext_offset + ext_len);

    // Payload starts after extension
    size_t payload_offset = ext_offset + ext_len;
    payload_.assign(data + payload_offset, data + size);
  } else {
    // No extension - payload starts after header
    size_t header_size = 12 + csrc_count_ * 4;
    payload_.assign(data + header_size, data + size);
  }

  // Rebuild buffer
  buffer_.assign(data, data + size);

  return 0;
}

int RtpPacket::DeserializeHeader(const uint8_t* data, size_t size) {
  if (size < 12) {
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
  if (version_ != 2) {
    return -1;  // Invalid version
  }

  // Check minimum size
  size_t min_size = 12 + csrc_count_ * 4;
  if (size < min_size) {
    return -1;
  }

  // Header fields
  seq_ = Read16(data + 2);
  timestamp_ = Read32(data + 4);
  ssrc_ = Read32(data + 8);

  // CSRC list
  csrc_list_.clear();
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
  packet->Serialize();  // Rebuild buffer
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
// Factory Functions
// ============================================================================

std::shared_ptr<RtpPacket> CreateRtpPacket() {
  return std::make_shared<RtpPacket>();
}

std::shared_ptr<RtpPacket> CreateRtpPacket(uint8_t payload_type,
                                            uint32_t timestamp,
                                            uint16_t seq) {
  return std::make_shared<RtpPacket>(payload_type, timestamp, seq);
}

}  // namespace minirtc
