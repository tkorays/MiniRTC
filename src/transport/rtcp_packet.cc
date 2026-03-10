/**
 * @file rtcp_packet.cc
 * @brief MiniRTC RTCP packet implementation
 */

#include "minirtc/transport/rtcp_packet.h"

#include <cstring>
#include <ctime>
#include <algorithm>

namespace minirtc {

// ============================================================================
// RtcpSrPacket Implementation
// ============================================================================

RtcpSrPacket::RtcpSrPacket() = default;

RtcpSrPacket::RtcpSrPacket(uint32_t ssrc) : sender_ssrc_(ssrc) {}

void RtcpSrPacket::AddReportBlock(const RtcpReportBlock& block) {
  if (report_blocks_.size() < 31) {
    report_blocks_.push_back(block);
  }
}

size_t RtcpSrPacket::GetSize() const {
  // Header (4) + SR (20) + Report blocks (24 each)
  return 4 + 20 + report_blocks_.size() * 24;
}

int RtcpSrPacket::Serialize(uint8_t* buffer, size_t size) const {
  size_t packet_size = GetSize();
  if (size < packet_size) {
    return -1;
  }

  // V=2 P=0 RC=report_count (5 bits each)
  buffer[0] = 0x80 | static_cast<uint8_t>(report_blocks_.size());
  buffer[1] = static_cast<uint8_t>(RtcpPacketType::kSR);
  Write16(buffer + 2, static_cast<uint16_t>(packet_size / 4));  // Length

  // Sender SSRC
  Write32(buffer + 4, sender_ssrc_);

  // NTP timestamp
  Write32(buffer + 8, ntp_timestamp_high_);
  Write32(buffer + 12, ntp_timestamp_low_);

  // RTP timestamp
  Write32(buffer + 16, rtp_timestamp_);

  // Sender packet count
  Write32(buffer + 20, sender_packet_count_);

  // Sender octet count
  Write32(buffer + 24, sender_octet_count_);

  // Report blocks
  size_t offset = 28;
  for (const auto& block : report_blocks_) {
    Write32(buffer + offset, block.ssrc);
    buffer[offset + 4] = block.fraction_lost;
    // Cumulative packets lost (24 bits)
    buffer[offset + 5] = static_cast<uint8_t>((block.packets_lost >> 16) & 0xFF);
    buffer[offset + 6] = static_cast<uint8_t>((block.packets_lost >> 8) & 0xFF);
    buffer[offset + 7] = static_cast<uint8_t>(block.packets_lost & 0xFF);
    Write32(buffer + offset + 8, block.highest_seq);
    Write32(buffer + offset + 12, block.jitter);
    Write32(buffer + offset + 16, block.lsr);
    Write32(buffer + offset + 20, block.dlsr);
    offset += 24;
  }

  return static_cast<int>(packet_size);
}

int RtcpSrPacket::Deserialize(const uint8_t* data, size_t size) {
  if (size < 28) {  // Minimum SR size
    return -1;
  }

  uint8_t rc = data[0] & 0x1F;
  uint16_t length = Read16(data + 2);
  size_t expected = 4 + (rc * 24);
  if (size < expected) {
    return -1;
  }

  sender_ssrc_ = Read32(data + 4);
  ntp_timestamp_high_ = Read32(data + 8);
  ntp_timestamp_low_ = Read32(data + 12);
  rtp_timestamp_ = Read32(data + 16);
  sender_packet_count_ = Read32(data + 20);
  sender_octet_count_ = Read32(data + 24);

  // Parse report blocks
  report_blocks_.clear();
  size_t offset = 28;
  for (uint8_t i = 0; i < rc; ++i) {
    RtcpReportBlock block;
    block.ssrc = Read32(data + offset);
    block.fraction_lost = data[offset + 4];
    block.packets_lost = (static_cast<int32_t>(data[offset + 5]) << 16) |
                         (static_cast<int32_t>(data[offset + 6]) << 8) |
                         static_cast<int32_t>(data[offset + 7]);
    block.highest_seq = Read32(data + offset + 8);
    block.jitter = Read32(data + offset + 12);
    block.lsr = Read32(data + offset + 16);
    block.dlsr = Read32(data + offset + 20);
    report_blocks_.push_back(block);
    offset += 24;
  }

  return 0;
}

void RtcpSrPacket::SetNtpTimestampNow() {
  // Get current time as NTP timestamp
  auto now = std::chrono::system_clock::now();
  auto epoch = now.time_since_epoch();
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
  auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(epoch).count();

  ntp_timestamp_high_ = static_cast<uint32_t>(seconds + 0x83AA7E80);  // NTP epoch offset
  ntp_timestamp_low_ = static_cast<uint32_t>(
      ((microseconds % 1000000) * 0xFFFFFFFFULL) / 1000000ULL);
}

uint64_t RtcpSrPacket::GetNtpTimestampUs() const {
  // Convert NTP timestamp to microseconds since epoch
  uint64_t seconds = ntp_timestamp_high_ - 0x83AA7E80;  // Remove NTP epoch offset
  uint64_t fraction = ntp_timestamp_low_;
  uint64_t microseconds = (fraction * 1000000ULL) >> 32;
  return seconds * 1000000ULL + microseconds;
}

// ============================================================================
// RtcpRrPacket Implementation
// ============================================================================

RtcpRrPacket::RtcpRrPacket() = default;

RtcpRrPacket::RtcpRrPacket(uint32_t ssrc) : sender_ssrc_(ssrc) {}

void RtcpRrPacket::AddReportBlock(const RtcpReportBlock& block) {
  if (report_blocks_.size() < 31) {
    report_blocks_.push_back(block);
  }
}

size_t RtcpRrPacket::GetSize() const {
  // Header (4) + RR (0) + Report blocks (24 each)
  return 4 + report_blocks_.size() * 24;
}

int RtcpRrPacket::Serialize(uint8_t* buffer, size_t size) const {
  size_t packet_size = GetSize();
  if (size < packet_size) {
    return -1;
  }

  // V=2 P=0 RC=report_count
  buffer[0] = 0x80 | static_cast<uint8_t>(report_blocks_.size());
  buffer[1] = static_cast<uint8_t>(RtcpPacketType::kRR);
  Write16(buffer + 2, static_cast<uint16_t>(packet_size / 4));

  // Sender SSRC
  Write32(buffer + 4, sender_ssrc_);

  // Report blocks
  size_t offset = 8;
  for (const auto& block : report_blocks_) {
    Write32(buffer + offset, block.ssrc);
    buffer[offset + 4] = block.fraction_lost;
    buffer[offset + 5] = static_cast<uint8_t>((block.packets_lost >> 16) & 0xFF);
    buffer[offset + 6] = static_cast<uint8_t>((block.packets_lost >> 8) & 0xFF);
    buffer[offset + 7] = static_cast<uint8_t>(block.packets_lost & 0xFF);
    Write32(buffer + offset + 8, block.highest_seq);
    Write32(buffer + offset + 12, block.jitter);
    Write32(buffer + offset + 16, block.lsr);
    Write32(buffer + offset + 20, block.dlsr);
    offset += 24;
  }

  return static_cast<int>(packet_size);
}

int RtcpRrPacket::Deserialize(const uint8_t* data, size_t size) {
  if (size < 8) {
    return -1;
  }

  uint8_t rc = data[0] & 0x1F;
  size_t expected = 4 + (rc * 24);
  if (size < expected) {
    return -1;
  }

  sender_ssrc_ = Read32(data + 4);

  report_blocks_.clear();
  size_t offset = 8;
  for (uint8_t i = 0; i < rc; ++i) {
    RtcpReportBlock block;
    block.ssrc = Read32(data + offset);
    block.fraction_lost = data[offset + 4];
    block.packets_lost = (static_cast<int32_t>(data[offset + 5]) << 16) |
                         (static_cast<int32_t>(data[offset + 6]) << 8) |
                         static_cast<int32_t>(data[offset + 7]);
    block.highest_seq = Read32(data + offset + 8);
    block.jitter = Read32(data + offset + 12);
    block.lsr = Read32(data + offset + 16);
    block.dlsr = Read32(data + offset + 20);
    report_blocks_.push_back(block);
    offset += 24;
  }

  return 0;
}

// ============================================================================
// RtcpSdesPacket Implementation
// ============================================================================

RtcpSdesPacket::RtcpSdesPacket() = default;

void RtcpSdesPacket::AddChunk(const SdesChunk& chunk) {
  if (chunks_.size() < 31) {
    chunks_.push_back(chunk);
  }
}

void RtcpSdesPacket::SetCname(uint32_t ssrc, const std::string& cname) {
  SdesChunk chunk;
  chunk.ssrc = ssrc;
  chunk.items.push_back({SdesItemType::kCNAME, cname});
  AddChunk(chunk);
}

void RtcpSdesPacket::SetName(uint32_t ssrc, const std::string& name) {
  // Find existing chunk or create new
  auto it = std::find_if(chunks_.begin(), chunks_.end(),
                          [ssrc](const SdesChunk& c) { return c.ssrc == ssrc; });
  if (it != chunks_.end()) {
    it->items.push_back({SdesItemType::kName, name});
  } else {
    SdesChunk chunk;
    chunk.ssrc = ssrc;
    chunk.items.push_back({SdesItemType::kName, name});
    AddChunk(chunk);
  }
}

size_t RtcpSdesPacket::GetSize() const {
  size_t size = 4;  // Header
  for (const auto& chunk : chunks_) {
    size += 4;  // SSRC
    for (const auto& item : chunk.items) {
      size += 2;  // Type + Length
      size += item.content.length();
    }
    size += 1;  // End marker
    // Padding
    if ((size % 4) != 0) {
      size += 4 - (size % 4);
    }
  }
  return size;
}

int RtcpSdesPacket::Serialize(uint8_t* buffer, size_t size) const {
  size_t packet_size = GetSize();
  if (size < packet_size) {
    return -1;
  }

  // Header
  buffer[0] = 0x80 | static_cast<uint8_t>(chunks_.size());
  buffer[1] = static_cast<uint8_t>(RtcpPacketType::kSDES);
  Write16(buffer + 2, static_cast<uint16_t>(packet_size / 4));

  size_t offset = 4;
  for (const auto& chunk : chunks_) {
    Write32(buffer + offset, chunk.ssrc);
    offset += 4;

    for (const auto& item : chunk.items) {
      buffer[offset++] = static_cast<uint8_t>(item.type);
      buffer[offset++] = static_cast<uint8_t>(item.content.length());
      memcpy(buffer + offset, item.content.data(), item.content.length());
      offset += item.content.length();
    }

    // End marker
    buffer[offset++] = 0;

    // Padding
    while ((offset % 4) != 0) {
      buffer[offset++] = 0;
    }
  }

  return static_cast<int>(packet_size);
}

int RtcpSdesPacket::Deserialize(const uint8_t* data, size_t size) {
  if (size < 4) {
    return -1;
  }

  uint8_t sc = data[0] & 0x1F;
  chunks_.clear();

  size_t offset = 4;
  for (uint8_t i = 0; i < sc && offset < size; ++i) {
    if (offset + 4 > size) break;

    SdesChunk chunk;
    chunk.ssrc = Read32(data + offset);
    offset += 4;

    while (offset < size && data[offset] != 0) {
      if (offset + 2 > size) break;

      SdesItem item;
      item.type = static_cast<SdesItemType>(data[offset++]);
      uint8_t item_len = data[offset++];

      if (offset + item_len > size) break;
      item.content.assign(reinterpret_cast<const char*>(data + offset),
                          item_len);
      offset += item_len;

      chunk.items.push_back(item);
    }

    if (offset < size && data[offset] == 0) {
      offset++;  // Skip end marker
    }

    // Skip padding
    while ((offset % 4) != 0 && offset < size) {
      offset++;
    }

    chunks_.push_back(chunk);
  }

  return 0;
}

// ============================================================================
// RtcpByePacket Implementation
// ============================================================================

RtcpByePacket::RtcpByePacket() = default;

void RtcpByePacket::SetReason(const std::string& reason) {
  reason_ = reason;
}

size_t RtcpByePacket::GetSize() const {
  // Header (4) + SSRCs + Reason
  size_t size = 4 + ssrcs_.size() * 4;
  if (!reason_.empty()) {
    size += 1 + 1 + reason_.length();  // Type + Length + Reason
  }
  // Padding
  if ((size % 4) != 0) {
    size += 4 - (size % 4);
  }
  return size;
}

int RtcpByePacket::Serialize(uint8_t* buffer, size_t size) const {
  size_t packet_size = GetSize();
  if (size < packet_size) {
    return -1;
  }

  // Header
  buffer[0] = 0x80 | static_cast<uint8_t>(ssrcs_.size());
  buffer[1] = static_cast<uint8_t>(RtcpPacketType::kBYE);
  Write16(buffer + 2, static_cast<uint16_t>(packet_size / 4));

  // SSRCs
  size_t offset = 4;
  for (uint32_t ssrc : ssrcs_) {
    Write32(buffer + offset, ssrc);
    offset += 4;
  }

  // Reason (optional)
  if (!reason_.empty()) {
    buffer[offset++] = static_cast<uint8_t>(SdesItemType::kNote);
    buffer[offset++] = static_cast<uint8_t>(reason_.length());
    memcpy(buffer + offset, reason_.data(), reason_.length());
    offset += reason_.length();
  }

  // Padding
  while ((offset % 4) != 0) {
    buffer[offset++] = 0;
  }

  return static_cast<int>(packet_size);
}

int RtcpByePacket::Deserialize(const uint8_t* data, size_t size) {
  if (size < 4) {
    return -1;
  }

  uint8_t sc = data[0] & 0x1F;
  ssrcs_.clear();

  size_t offset = 4;
  for (uint8_t i = 0; i < sc && offset + 4 <= size; ++i) {
    ssrcs_.push_back(Read32(data + offset));
    offset += 4;
  }

  // Reason (optional)
  if (offset < size && data[offset] != 0) {
    if (offset + 2 <= size) {
      offset++;  // Skip type
      uint8_t len = data[offset++];
      if (offset + len <= size) {
        reason_.assign(reinterpret_cast<const char*>(data + offset), len);
      }
    }
  }

  return 0;
}

// ============================================================================
// RtcpNackPacket Implementation
// ============================================================================

RtcpNackPacket::RtcpNackPacket() = default;

RtcpNackPacket::RtcpNackPacket(uint32_t sender_ssrc, uint32_t media_ssrc)
    : sender_ssrc_(sender_ssrc), media_ssrc_(media_ssrc) {}

void RtcpNackPacket::AddNack(uint16_t seq) {
  nack_list_.push_back(seq);
}

void RtcpNackPacket::AddNackRange(uint16_t start, uint16_t end) {
  for (uint16_t seq = start; seq != end; ++seq) {
    nack_list_.push_back(seq);
  }
}

size_t RtcpNackPacket::GetSize() const {
  // Header (4) + FCI (4 + 2 * nack_count)
  size_t size = 8 + nack_list_.size() * 2;
  // Padding
  if ((size % 4) != 0) {
    size += 4 - (size % 4);
  }
  return size;
}

int RtcpNackPacket::Serialize(uint8_t* buffer, size_t size) const {
  size_t packet_size = GetSize();
  if (size < packet_size) {
    return -1;
  }

  // Header
  buffer[0] = 0x80;  // V=2, P=0
  buffer[1] = static_cast<uint8_t>(RtcpPacketType::kRTPFB);
  Write16(buffer + 2, static_cast<uint16_t>(packet_size / 4 - 1));

  // Sender SSRC
  Write32(buffer + 4, sender_ssrc_);

  // Media SSRC
  Write32(buffer + 8, media_ssrc_);

  // NACK list (PID + BITMASK)
  size_t offset = 12;
  uint16_t base_seq = 0;
  bool first = true;
  uint8_t bitmask = 0;
  uint8_t bitmask_offset = 0;

  for (size_t i = 0; i < nack_list_.size(); ++i) {
    if (first) {
      base_seq = nack_list_[i];
      Write16(buffer + offset, base_seq);
      offset += 2;
      first = false;
      bitmask = 0;
      bitmask_offset = offset;
      offset += 1;
    } else {
      int16_t diff = static_cast<int16_t>(nack_list_[i] - base_seq);
      if (diff >= 1 && diff <= 16) {
        bitmask |= (1 << (diff - 1));
        buffer[bitmask_offset] = bitmask;
      } else {
        // New NACK
        buffer[bitmask_offset] = bitmask;
        base_seq = nack_list_[i];
        Write16(buffer + offset, base_seq);
        offset += 2;
        bitmask = 0;
        bitmask_offset = offset;
        offset += 1;
      }
    }
  }

  // Last bitmask
  if (!first && bitmask_offset < offset) {
    buffer[bitmask_offset] = bitmask;
  }

  // Padding
  while ((offset % 4) != 0) {
    offset++;
  }

  return static_cast<int>(offset);
}

int RtcpNackPacket::Deserialize(const uint8_t* data, size_t size) {
  if (size < 12) {
    return -1;
  }

  sender_ssrc_ = Read32(data + 4);
  media_ssrc_ = Read32(data + 8);

  nack_list_.clear();
  size_t offset = 12;

  while (offset + 2 <= size) {
    uint16_t pid = Read16(data + offset);
    nack_list_.push_back(pid);
    offset += 2;

    if (offset < size) {
      uint8_t bitmask = data[offset++];
      for (int i = 0; i < 8; ++i) {
        if (bitmask & (1 << i)) {
          nack_list_.push_back(pid + i + 1);
        }
      }
    }

    // Align to 32-bit boundary
    while ((offset % 4) != 0 && offset < size) {
      offset++;
    }
  }

  return 0;
}

// ============================================================================
// RtcpCompoundPacket Implementation
// ============================================================================

void RtcpCompoundPacket::AddPacket(std::shared_ptr<RtcpPacket> packet) {
  packets_.push_back(packet);
}

size_t RtcpCompoundPacket::GetSize() const {
  size_t size = 0;
  for (const auto& packet : packets_) {
    size += packet->GetSize();
  }
  return size;
}

int RtcpCompoundPacket::Serialize(uint8_t* buffer, size_t size) const {
  size_t total_size = GetSize();
  if (size < total_size) {
    return -1;
  }

  size_t offset = 0;
  for (const auto& packet : packets_) {
    int result = packet->Serialize(buffer + offset, size - offset);
    if (result < 0) {
      return result;
    }
    offset += result;
  }

  return static_cast<int>(offset);
}

int RtcpCompoundPacket::Deserialize(const uint8_t* data, size_t size) {
  packets_.clear();

  size_t offset = 0;
  while (offset + 4 <= size) {
    uint8_t pt = data[offset + 1];
    uint16_t length = (static_cast<uint16_t>(data[offset + 2]) << 8) | data[offset + 3];
    if (offset + 4 + length > size) {
      break;
    }

    std::shared_ptr<RtcpPacket> packet;
    switch (static_cast<RtcpPacketType>(pt)) {
      case RtcpPacketType::kSR:
        packet = std::make_shared<RtcpSrPacket>();
        break;
      case RtcpPacketType::kRR:
        packet = std::make_shared<RtcpRrPacket>();
        break;
      case RtcpPacketType::kSDES:
        packet = std::make_shared<RtcpSdesPacket>();
        break;
      case RtcpPacketType::kBYE:
        packet = std::make_shared<RtcpByePacket>();
        break;
      case RtcpPacketType::kRTPFB:
        packet = std::make_shared<RtcpNackPacket>();
        break;
      default:
        // Unknown packet type, skip
        offset += 4 + length;
        continue;
    }

    if (packet->Deserialize(data + offset, 4 + length) == 0) {
      packets_.push_back(packet);
    }

    offset += 4 + length;
  }

  return packets_.empty() ? -1 : 0;
}

// ============================================================================
// Factory Functions
// ============================================================================

std::shared_ptr<RtcpSrPacket> CreateRtcpSr(uint32_t ssrc) {
  return std::make_shared<RtcpSrPacket>(ssrc);
}

std::shared_ptr<RtcpRrPacket> CreateRtcpRr(uint32_t ssrc) {
  return std::make_shared<RtcpRrPacket>(ssrc);
}

std::shared_ptr<RtcpSdesPacket> CreateRtcpSdes() {
  return std::make_shared<RtcpSdesPacket>();
}

std::shared_ptr<RtcpByePacket> CreateRtcpBye(const std::vector<uint32_t>& ssrcs,
                                              const std::string& reason) {
  auto packet = std::make_shared<RtcpByePacket>();
  for (uint32_t ssrc : ssrcs) {
    packet->AddSsrc(ssrc);
  }
  packet->SetReason(reason);
  return packet;
}

std::shared_ptr<RtcpNackPacket> CreateRtcpNack(uint32_t sender_ssrc,
                                                uint32_t media_ssrc,
                                                const std::vector<uint16_t>& nacks) {
  auto packet = std::make_shared<RtcpNackPacket>(sender_ssrc, media_ssrc);
  for (uint16_t seq : nacks) {
    packet->AddNack(seq);
  }
  return packet;
}

std::shared_ptr<RtcpCompoundPacket> CreateRtcpCompound() {
  return std::make_shared<RtcpCompoundPacket>();
}

}  // namespace minirtc
