/**
 * @file nack_module.cc
 * @brief Implementation of NackModule class
 */

#include "minirtc/nack_module.h"

#include <algorithm>
#include <cstdint>

namespace minirtc {

// ============================================================================
// NackModule Implementation
// ============================================================================

NackModule::NackModule()
    : initialized_(false),
      running_(false),
      last_nack_send_time_ms_(0) {
  // Create packet cache with default config
  PacketCacheConfig cache_config;
  cache_config.max_cache_size = 512;
  cache_config.max_age_ms = 5000;
  packet_cache_ = std::make_shared<PacketCache>(cache_config);
}

bool NackModule::Initialize(const NackConfig& config) {
  if (initialized_) {
    return false;
  }

  config_ = config;
  
  // Update packet cache config
  PacketCacheConfig cache_config;
  cache_config.max_cache_size = static_cast<size_t>(config.max_nack_list_size);
  cache_config.max_age_ms = config.nack_timeout_ms * 5;  // 5x timeout
  packet_cache_ = std::make_shared<PacketCache>(cache_config);

  // Set up packet lost callback to add to NACK list
  packet_cache_->SetOnPacketLostCallback([this](uint16_t seq_num) {
    if (config_.enable_nack) {
      AddToNackList(seq_num, last_nack_send_time_ms_);
    }
  });

  initialized_ = true;
  return true;
}

void NackModule::Start() {
  if (!initialized_ || running_) {
    return;
  }
  running_ = true;
}

void NackModule::Stop() {
  running_ = false;
}

void NackModule::Reset() {
  nack_list_.clear();
  stats_ = NackStatistics();
  packet_cache_->Clear();
  last_nack_send_time_ms_ = 0;
}

void NackModule::OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) {
  if (!packet || !running_) {
    return;
  }

  uint16_t seq_num = packet->GetSequenceNumber();
  int64_t current_time = last_nack_send_time_ms_;

  // Insert packet into cache and detect lost packets
  std::vector<uint16_t> lost_packets = packet_cache_->OnPacketArrived(seq_num, current_time);
  
  // Insert the packet itself
  packet_cache_->InsertPacket(packet);

  // Remove from NACK list if it was previously marked as lost
  RemoveFromNackList(seq_num);
}

void NackModule::OnRtxPacketReceived(std::shared_ptr<RtpPacket> packet) {
  if (!packet || !running_) {
    return;
  }

  stats_.rtx_packets_received++;

  uint16_t seq_num = packet->GetSequenceNumber();
  int64_t current_time = last_nack_send_time_ms_;

  // Handle RTX response - try to recover
  bool recovered = HandleRtxResponse(seq_num);

  if (recovered) {
    stats_.rtx_success_count++;
  }

  // Insert RTX packet into cache
  PacketCacheItem item;
  item.packet = packet;
  item.received_time_ms = current_time;
  item.is_retransmission = true;
  item.ref_count = 0;
  
  packet_cache_->InsertPacket(packet);
}

std::vector<uint16_t> NackModule::GetNackList(int64_t current_time_ms) {
  if (!running_) {
    return {};
  }

  last_nack_send_time_ms_ = current_time_ms;
  std::vector<uint16_t> nack_list;

  // Update NACK status and collect packets that need NACK
  for (auto& entry : nack_list_) {
    NackStatus& status = entry.second;
    
    // Update status
    UpdateNackStatus(entry.first, current_time_ms);
    
    // Check if should send NACK
    if (ShouldSendNack(status, current_time_ms)) {
      nack_list.push_back(entry.first);
      status.last_send_time_ms = current_time_ms;
      status.retries++;
    }
  }

  // Remove packets that have exceeded max retries
  auto it = nack_list_.begin();
  while (it != nack_list_.end()) {
    if (it->second.retries >= config_.max_retransmissions) {
      it = nack_list_.erase(it);
      stats_.rtx_timeout_count++;
    } else {
      ++it;
    }
  }

  stats_.current_nack_list_size = static_cast<uint32_t>(nack_list_.size());

  return nack_list;
}

void NackModule::RemoveFromNackList(uint16_t seq_num) {
  nack_list_.erase(seq_num);
}

bool NackModule::IsInNackList(uint16_t seq_num) const {
  return nack_list_.find(seq_num) != nack_list_.end();
}

bool NackModule::HandleRtxResponse(uint16_t seq_num) {
  // Check if we have the packet in cache
  auto packet = packet_cache_->GetPacket(seq_num);
  if (packet) {
    // Remove from NACK list
    RemoveFromNackList(seq_num);
    return true;
  }
  return false;
}

void NackModule::SetConfig(const NackConfig& config) {
  config_ = config;
}

NackConfig NackModule::GetConfig() const {
  return config_;
}

NackStatistics NackModule::GetStatistics() const {
  NackStatistics stats = stats_;
  stats.current_nack_list_size = static_cast<uint32_t>(nack_list_.size());
  return stats;
}

void NackModule::ResetStatistics() {
  stats_ = NackStatistics();
}

void NackModule::SetOnNackRequestCallback(OnNackRequestCallback callback) {
  on_nack_request_ = std::move(callback);
}

void NackModule::SetOnRtxPacketCallback(OnRtxPacketCallback callback) {
  on_rtx_packet_ = std::move(callback);
}

bool NackModule::IsEnabled() const {
  return config_.enable_nack && initialized_;
}

int NackModule::GetRttEstimate() const {
  return config_.rtt_estimate_ms;
}

void NackModule::AddToNackList(uint16_t seq_num, int64_t current_time_ms) {
  // Don't add if already in list
  if (nack_list_.find(seq_num) != nack_list_.end()) {
    return;
  }

  // Don't add if list is full
  if (static_cast<int>(nack_list_.size()) >= config_.max_nack_list_size) {
    return;
  }

  NackStatus status;
  status.sequence_number = seq_num;
  status.send_time_ms = current_time_ms;
  status.last_send_time_ms = current_time_ms;
  status.retries = 0;
  status.at_risk = true;

  nack_list_[seq_num] = status;
}

void NackModule::UpdateNackStatus(uint16_t seq_num, int64_t current_time_ms) {
  auto it = nack_list_.find(seq_num);
  if (it == nack_list_.end()) {
    return;
  }

  NackStatus& status = it->second;
  
  // Check if packet has been received
  if (packet_cache_->HasPacket(seq_num)) {
    // Packet recovered, remove from NACK list
    nack_list_.erase(it);
    stats_.rtx_success_count++;
  }
}

bool NackModule::ShouldSendNack(const NackStatus& status, int64_t current_time_ms) const {
  // Check if max retries exceeded
  if (status.retries >= config_.max_retransmissions) {
    return false;
  }

  // Check timeout
  int64_t time_since_last = current_time_ms - status.last_send_time_ms;
  if (time_since_last < config_.nack_timeout_ms) {
    // Check if this is first request
    if (status.retries > 0) {
      return false;
    }
  }

  return true;
}

// ============================================================================
// Factory Implementation
// ============================================================================

std::unique_ptr<INackModule> NackModuleFactory::Create() {
  return std::make_unique<NackModule>();
}

}  // namespace minirtc
