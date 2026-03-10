/**
 * @file fec_module.cc
 * @brief Implementation of FecModule class
 */

#include "minirtc/fec_module.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace minirtc {

// ============================================================================
// XorFecEncoder Implementation
// ============================================================================

bool XorFecEncoder::AddPacket(std::shared_ptr<RtpPacket> packet) {
  if (!packet) {
    return false;
  }
  packets_.push_back(packet);
  return true;
}

std::shared_ptr<RtpPacket> XorFecEncoder::Encode() {
  if (packets_.empty()) {
    return nullptr;
  }

  // Find the maximum packet size
  size_t max_size = 0;
  for (const auto& packet : packets_) {
    if (packet && packet->GetPayloadSize() > max_size) {
      max_size = packet->GetPayloadSize();
    }
  }

  if (max_size == 0) {
    return nullptr;
  }

  // Create FEC packet
  auto fec_packet = std::make_shared<RtpPacket>();
  
  // Set FEC header (using RTP extension for FEC info)
  // For XOR FEC, we store the protected sequence numbers in the payload
  
  // Calculate XOR of all packets
  std::vector<uint8_t> xor_data(max_size, 0);
  
  for (const auto& packet : packets_) {
    if (!packet) continue;
    
    const uint8_t* data = packet->GetPayload();
    size_t payload_size = packet->GetPayloadSize();
    
    for (size_t i = 0; i < payload_size && i < max_size; ++i) {
      xor_data[i] ^= data[i];
    }
  }

  // Set payload to FEC packet
  fec_packet->SetPayloadType(97);  // FEC payload type
  fec_packet->SetTimestamp(packets_.front()->GetTimestamp());
  fec_packet->SetSequenceNumber(packets_.front()->GetSequenceNumber());
  
  // Store protected sequence numbers in FEC payload
  // First 2 bytes: number of protected packets
  // Next: sequence numbers (2 bytes each)
  size_t header_size = 2 + packets_.size() * 2;
  std::vector<uint8_t> fec_payload(header_size + max_size);
  
  // Store count
  fec_payload[0] = static_cast<uint8_t>(packets_.size() & 0xFF);
  fec_payload[1] = static_cast<uint8_t>((packets_.size() >> 8) & 0xFF);
  
  // Store sequence numbers
  size_t offset = 2;
  for (const auto& packet : packets_) {
    uint16_t seq = packet->GetSequenceNumber();
    fec_payload[offset++] = static_cast<uint8_t>(seq & 0xFF);
    fec_payload[offset++] = static_cast<uint8_t>((seq >> 8) & 0xFF);
  }
  
  // Copy XOR data
  std::memcpy(fec_payload.data() + header_size, xor_data.data(), max_size);
  
  fec_packet->SetPayload(fec_payload.data() + header_size, max_size);
  fec_packet->SetExtension(true);

  return fec_packet;
}

void XorFecEncoder::Clear() {
  packets_.clear();
}

// ============================================================================
// XorFecDecoder Implementation
// ============================================================================

void XorFecDecoder::AddFecPacket(std::shared_ptr<RtpPacket> fec_packet) {
  if (fec_packet) {
    fec_packet_ = fec_packet;
  }
}

void XorFecDecoder::AddMediaPacket(std::shared_ptr<RtpPacket> media_packet) {
  if (media_packet) {
    media_packets_.push_back(media_packet);
  }
}

std::shared_ptr<RtpPacket> XorFecDecoder::TryRecover(uint16_t seq_num) {
  if (!fec_packet_ || media_packets_.empty()) {
    return nullptr;
  }

  // Check if this sequence number is in the protected list
  const uint8_t* payload = fec_packet_->GetPayload();
  size_t payload_size = fec_packet_->GetPayloadSize();
  
  if (payload_size < 2) {
    return nullptr;
  }

  // Read protected packet count
  uint16_t count = static_cast<uint16_t>(payload[0]) | 
                   (static_cast<uint16_t>(payload[1]) << 8);
  
  if (payload_size < 2 + count * 2) {
    return nullptr;
  }

  // Check if seq_num is in protected list
  bool found = false;
  std::vector<uint16_t> protected_seqs;
  
  for (uint16_t i = 0; i < count; ++i) {
    uint16_t seq = static_cast<uint16_t>(payload[2 + i * 2]) |
                   (static_cast<uint16_t>(payload[2 + i * 2 + 1]) << 8);
    protected_seqs.push_back(seq);
    if (seq == seq_num) {
      found = true;
      break;
    }
  }

  if (!found) {
    return nullptr;
  }

  // Check if we have all other packets (can recover this one)
  std::map<uint16_t, std::shared_ptr<RtpPacket>> packet_map;
  for (const auto& p : media_packets_) {
    packet_map[p->GetSequenceNumber()] = p;
  }

  // Check which packets we have
  for (uint16_t protected_seq : protected_seqs) {
    if (packet_map.find(protected_seq) == packet_map.end() && protected_seq != seq_num) {
      // Missing another packet, cannot recover
      return nullptr;
    }
  }

  // We can recover - perform XOR
  size_t fec_data_offset = 2 + count * 2;
  size_t fec_data_size = payload_size - fec_data_offset;
  
  if (fec_data_size == 0) {
    return nullptr;
  }

  std::vector<uint8_t> recovered_data(fec_data_size, 0);
  
  // Start with FEC data
  std::memcpy(recovered_data.data(), payload + fec_data_offset, fec_data_size);
  
  // XOR with all available packets
  for (const auto& p : media_packets_) {
    if (p->GetSequenceNumber() == seq_num) {
      continue;  // Skip the packet we're recovering
    }
    
    const uint8_t* data = p->GetPayload();
    size_t data_size = std::min(p->GetPayloadSize(), fec_data_size);
    
    for (size_t i = 0; i < data_size; ++i) {
      recovered_data[i] ^= data[i];
    }
  }

  // Create recovered packet
  auto recovered = std::make_shared<RtpPacket>();
  recovered->SetPayloadType(fec_packet_->GetPayloadType());
  recovered->SetTimestamp(fec_packet_->GetTimestamp());
  recovered->SetSequenceNumber(seq_num);
  recovered->SetPayload(recovered_data.data(), fec_data_size);

  return recovered;
}

std::vector<std::shared_ptr<RtpPacket>> XorFecDecoder::RecoverAll() {
  std::vector<std::shared_ptr<RtpPacket>> recovered;

  if (!fec_packet_) {
    return recovered;
  }

  // Get protected sequence numbers
  const uint8_t* payload = fec_packet_->GetPayload();
  size_t payload_size = fec_packet_->GetPayloadSize();
  
  if (payload_size < 2) {
    return recovered;
  }

  uint16_t count = static_cast<uint16_t>(payload[0]) | 
                   (static_cast<uint16_t>(payload[1]) << 8);

  // Try to recover each protected packet
  for (uint16_t i = 0; i < count; ++i) {
    uint16_t seq = static_cast<uint16_t>(payload[2 + i * 2]) |
                   (static_cast<uint16_t>(payload[2 + i * 2 + 1]) << 8);
    
    // Check if we already have this packet
    bool have_packet = false;
    for (const auto& p : media_packets_) {
      if (p->GetSequenceNumber() == seq) {
        have_packet = true;
        break;
      }
    }
    
    if (!have_packet) {
      auto recovered_packet = TryRecover(seq);
      if (recovered_packet) {
        recovered.push_back(recovered_packet);
      }
    }
  }

  return recovered;
}

void XorFecDecoder::Clear() {
  media_packets_.clear();
  fec_packet_ = nullptr;
}

// ============================================================================
// FecModule Implementation
// ============================================================================

FecModule::FecModule()
    : initialized_(false),
      running_(false),
      current_group_start_seq_(0),
      last_group_time_ms_(0) {
}

bool FecModule::Initialize(const FecConfig& config) {
  if (initialized_) {
    return false;
  }

  config_ = config;
  initialized_ = true;
  return true;
}

void FecModule::Start() {
  if (!initialized_ || running_) {
    return;
  }
  running_ = true;
}

void FecModule::Stop() {
  running_ = false;
}

void FecModule::Reset() {
  encoder_.Clear();
  decoder_.Clear();
  pending_fec_packets_.clear();
  received_media_packets_.clear();
  received_fec_packets_.clear();
  stats_ = FecStatistics();
  current_group_start_seq_ = 0;
  last_group_time_ms_ = 0;
}

bool FecModule::AddMediaPacket(std::shared_ptr<RtpPacket> packet) {
  if (!packet || !running_) {
    return false;
  }

  // Add to encoder
  encoder_.AddPacket(packet);

  // Check if we should trigger FEC encoding
  // For simplicity, encode when group size reaches threshold
  int group_size = static_cast<int>(config_.fec_percentage * encoder_.group_size() / 100.0);
  if (group_size > 0 && encoder_.group_size() >= static_cast<size_t>(group_size)) {
    EncodeFec();
  }

  return true;
}

std::vector<std::shared_ptr<RtpPacket>> FecModule::EncodeFec() {
  std::vector<std::shared_ptr<RtpPacket>> fec_packets;

  if (!running_ || !encoder_.can_encode()) {
    return fec_packets;
  }

  // Generate FEC packet
  auto fec_packet = encoder_.Encode();
  if (fec_packet) {
    pending_fec_packets_.push_back(fec_packet);
    fec_packets.push_back(fec_packet);
    stats_.fec_packets_sent++;
  }

  // Clear encoder for next group
  encoder_.Clear();

  // Trigger callback
  if (on_fec_encode_) {
    on_fec_encode_(fec_packets);
  }

  return fec_packets;
}

std::vector<std::shared_ptr<RtpPacket>> FecModule::GetPendingFecPackets() {
  return pending_fec_packets_;
}

void FecModule::ClearPendingFecPackets() {
  pending_fec_packets_.clear();
}

std::vector<std::shared_ptr<RtpPacket>> FecModule::OnRtpPacketReceived(
    std::shared_ptr<RtpPacket> packet) {
  std::vector<std::shared_ptr<RtpPacket>> recovered;

  if (!packet || !running_) {
    return recovered;
  }

  // Check if this is an FEC packet
  if (IsFecPacket(packet)) {
    OnFecPacketReceived(packet);
    return recovered;
  }

  // This is a media packet - add to decoder
  uint16_t seq_num = packet->GetSequenceNumber();
  received_media_packets_[seq_num] = packet;
  decoder_.AddMediaPacket(packet);

  // Try to recover packets
  recovered = decoder_.RecoverAll();

  // Update statistics
  for (const auto& rec : recovered) {
    stats_.packets_recovered++;
    stats_.recovery_success_count++;
    
    // Add recovered packet to received packets
    if (rec) {
      received_media_packets_[rec->GetSequenceNumber()] = rec;
    }
  }

  // Trigger callback
  if (!recovered.empty() && on_fec_recover_) {
    on_fec_recover_(recovered);
  }

  return recovered;
}

void FecModule::OnFecPacketReceived(std::shared_ptr<RtpPacket> packet) {
  if (!packet || !running_) {
    return;
  }

  stats_.fec_packets_received++;
  received_fec_packets_.push_back(packet);
  decoder_.AddFecPacket(packet);
}

std::shared_ptr<RtpPacket> FecModule::TryRecoverPacket(uint16_t missing_seq) {
  if (!running_) {
    return nullptr;
  }

  // Check if we already have this packet
  auto it = received_media_packets_.find(missing_seq);
  if (it != received_media_packets_.end()) {
    return it->second;
  }

  // Try to recover using decoder
  auto recovered = decoder_.TryRecover(missing_seq);
  
  if (recovered) {
    stats_.packets_recovered++;
    stats_.recovery_success_count++;
    received_media_packets_[missing_seq] = recovered;
  } else {
    stats_.recovery_failure_count++;
  }

  return recovered;
}

std::vector<std::shared_ptr<RtpPacket>> FecModule::TryDecodeAll() {
  if (!running_) {
    return {};
  }

  std::vector<std::shared_ptr<RtpPacket>> recovered = decoder_.RecoverAll();

  for (const auto& rec : recovered) {
    if (rec) {
      stats_.packets_recovered++;
      stats_.recovery_success_count++;
      received_media_packets_[rec->GetSequenceNumber()] = rec;
    }
  }

  if (!recovered.empty() && on_fec_recover_) {
    on_fec_recover_(recovered);
  }

  return recovered;
}

void FecModule::SetConfig(const FecConfig& config) {
  config_ = config;
}

FecConfig FecModule::GetConfig() const {
  return config_;
}

void FecModule::UpdateFecLevel(FecLevel level) {
  config_.fec_level = level;
  
  // Update FEC percentage based on level
  switch (level) {
    case FecLevel::kLow:
      config_.fec_percentage = 10;
      break;
    case FecLevel::kMedium:
      config_.fec_percentage = 15;
      break;
    case FecLevel::kHigh:
      config_.fec_percentage = 25;
      break;
    case FecLevel::kUltra:
      config_.fec_percentage = 40;
      break;
  }
}

FecStatistics FecModule::GetStatistics() const {
  FecStatistics stats = stats_;
  
  // Calculate current FEC percentage
  size_t media_count = received_media_packets_.size();
  if (media_count > 0) {
    stats.current_fec_percentage = 
        static_cast<float>(stats.fec_packets_received) / 
        static_cast<float>(media_count) * 100.0f;
  }
  
  return stats;
}

void FecModule::ResetStatistics() {
  stats_ = FecStatistics();
}

void FecModule::SetOnFecEncodeCallback(OnFecEncodeCallback callback) {
  on_fec_encode_ = std::move(callback);
}

void FecModule::SetOnFecRecoverCallback(OnFecRecoverCallback callback) {
  on_fec_recover_ = std::move(callback);
}

bool FecModule::IsEnabled() const {
  return config_.enable_fec && initialized_;
}

std::vector<FecGroup> FecModule::GetFecGroups() const {
  std::vector<FecGroup> groups;
  
  // Simple implementation - return current group info
  FecGroup group;
  group.group_id = 1;
  group.start_seq = current_group_start_seq_;
  group.media_count = static_cast<int>(received_media_packets_.size());
  group.fec_count = static_cast<int>(received_fec_packets_.size());
  group.complete = !received_media_packets_.empty() && 
                   !received_fec_packets_.empty();
  
  groups.push_back(group);
  
  return groups;
}

std::shared_ptr<RtpPacket> FecModule::CreateFecPacket(
    const std::vector<std::shared_ptr<RtpPacket>>& packets) {
  // Use encoder to create FEC packet
  for (const auto& p : packets) {
    encoder_.AddPacket(p);
  }
  
  auto fec_packet = encoder_.Encode();
  encoder_.Clear();
  
  return fec_packet;
}

bool FecModule::IsFecPacket(std::shared_ptr<RtpPacket> packet) const {
  if (!packet) {
    return false;
  }
  
  // Check if payload type matches FEC payload type
  return packet->GetPayloadType() == config_.fec_payload_type;
}

std::vector<uint16_t> FecModule::GetProtectedSeqNums(
    std::shared_ptr<RtpPacket> fec_packet) const {
  std::vector<uint16_t> seq_nums;
  
  if (!fec_packet) {
    return seq_nums;
  }
  
  const uint8_t* payload = fec_packet->GetPayload();
  size_t payload_size = fec_packet->GetPayloadSize();
  
  if (payload_size < 2) {
    return seq_nums;
  }
  
  uint16_t count = static_cast<uint16_t>(payload[0]) | 
                   (static_cast<uint16_t>(payload[1]) << 8);
  
  for (uint16_t i = 0; i < count && (2 + i * 2 + 1) < payload_size; ++i) {
    uint16_t seq = static_cast<uint16_t>(payload[2 + i * 2]) |
                   (static_cast<uint16_t>(payload[2 + i * 2 + 1]) << 8);
    seq_nums.push_back(seq);
  }
  
  return seq_nums;
}

// ============================================================================
// Factory Implementation
// ============================================================================

std::unique_ptr<IFecModule> FecModuleFactory::Create(FecAlgorithm algorithm) {
  switch (algorithm) {
    case FecAlgorithm::kXorFec:
      return std::make_unique<FecModule>();
    case FecAlgorithm::kUlpFec:
    case FecAlgorithm::kHybrid:
    case FecAlgorithm::kNone:
    default:
      return std::make_unique<FecModule>();  // Default to XOR
  }
}

std::unique_ptr<IFecModule> FecModuleFactory::CreateUlpFec() {
  return std::make_unique<FecModule>();
}

std::unique_ptr<IFecModule> FecModuleFactory::CreateXorFec() {
  return std::make_unique<FecModule>();
}

}  // namespace minirtc
