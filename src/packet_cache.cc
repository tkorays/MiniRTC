/**
 * @file packet_cache.cc
 * @brief Implementation of PacketCache class
 */

#include "minirtc/packet_cache.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace minirtc {

// Helper function to compare sequence numbers considering wrap-around
static bool IsNewerSequenceNumber(uint16_t new_seq, uint16_t old_seq) {
  // Handle sequence number wrap-around
  int16_t diff = static_cast<int16_t>(new_seq - old_seq);
  return diff > 0 && diff < 32768;
}

// ============================================================================
// PacketCache Implementation
// ============================================================================

PacketCache::PacketCache(const PacketCacheConfig& config)
    : config_(config),
      highest_seq_num_(0),
      last_update_time_ms_(0),
      initialized_(false) {
  // Initialize with a reasonable default
  if (config_.max_cache_size == 0) {
    config_.max_cache_size = 512;
  }
  if (config_.max_age_ms == 0) {
    config_.max_age_ms = 5000;
  }
  initialized_ = true;
}

bool PacketCache::InsertPacket(std::shared_ptr<RtpPacket> packet) {
  if (!packet || !initialized_) {
    return false;
  }

  uint16_t seq_num = packet->GetSequenceNumber();

  // Check if packet already exists
  if (cache_.find(seq_num) != cache_.end()) {
    // Update existing entry
    cache_[seq_num].packet = packet;
    cache_[seq_num].received_time_ms = last_update_time_ms_;
    return true;
  }

  // Check cache size limit
  if (cache_.size() >= config_.max_cache_size) {
    // Remove oldest packet
    auto oldest_it = cache_.begin();
    if (oldest_it != cache_.end()) {
      cache_.erase(oldest_it);
    }
  }

  // Insert new packet
  PacketCacheItem item;
  item.packet = packet;
  item.received_time_ms = last_update_time_ms_;
  item.is_retransmission = false;
  item.ref_count = 0;

  cache_[seq_num] = std::move(item);

  // Update highest sequence number
  if (highest_seq_num_ == 0 || IsNewerSequenceNumber(seq_num, highest_seq_num_)) {
    highest_seq_num_ = seq_num;
  }

  stats_.total_packets_received++;
  stats_.current_seq = seq_num;
  stats_.last_packet_time_ms = last_update_time_ms_;

  return true;
}

std::shared_ptr<RtpPacket> PacketCache::GetPacket(uint16_t seq_num) {
  auto it = cache_.find(seq_num);
  if (it != cache_.end()) {
    return it->second.packet;
  }
  return nullptr;
}

bool PacketCache::HasPacket(uint16_t seq_num) const {
  return cache_.find(seq_num) != cache_.end();
}

bool PacketCache::RemovePacket(uint16_t seq_num) {
  auto it = cache_.find(seq_num);
  if (it != cache_.end()) {
    cache_.erase(it);
    return true;
  }
  return false;
}

std::vector<uint16_t> PacketCache::OnPacketArrived(uint16_t seq_num,
                                                    int64_t current_time_ms) {
  std::vector<uint16_t> lost_packets;
  last_update_time_ms_ = current_time_ms;

  // Check if packet is old/duplicate
  if (IsOldPacket(seq_num, current_time_ms)) {
    return lost_packets;
  }

  // Calculate expected sequence number
  uint16_t expected_seq = GetExpectedSequenceNumber();

  // Detect lost packets
  if (expected_seq != 0 && IsNewerSequenceNumber(seq_num, expected_seq)) {
    // There are lost packets between expected and received
    uint16_t current = expected_seq;
    while (IsNewerSequenceNumber(seq_num, current)) {
      // Check if packet is not in cache
      if (cache_.find(current) == cache_.end()) {
        lost_packets.push_back(current);
        HandlePacketLost(current);
      }
      current = static_cast<uint16_t>(current + 1);
    }
  }

  return lost_packets;
}

std::vector<std::shared_ptr<RtpPacket>> PacketCache::GetPacketsInRange(
    uint16_t start_seq, uint16_t end_seq) const {
  std::vector<std::shared_ptr<RtpPacket>> packets;

  uint16_t current = start_seq;
  while (IsNewerSequenceNumber(end_seq, current) || current == end_seq) {
    auto it = cache_.find(current);
    if (it != cache_.end() && it->second.packet) {
      packets.push_back(it->second.packet);
    }
    if (current == end_seq) break;
    current = static_cast<uint16_t>(current + 1);
  }

  return packets;
}

std::vector<std::shared_ptr<RtpPacket>> PacketCache::GetAllPackets() const {
  std::vector<std::shared_ptr<RtpPacket>> packets;

  for (const auto& entry : cache_) {
    if (entry.second.packet) {
      packets.push_back(entry.second.packet);
    }
  }

  return packets;
}

void PacketCache::Clear() {
  cache_.clear();
  highest_seq_num_ = 0;
  last_update_time_ms_ = 0;
}

size_t PacketCache::CleanupExpiredPackets(int64_t current_time_ms) {
  size_t cleaned = 0;

  auto it = cache_.begin();
  while (it != cache_.end()) {
    int64_t age = current_time_ms - it->second.received_time_ms;
    if (age > config_.max_age_ms) {
      it = cache_.erase(it);
      cleaned++;
    } else {
      ++it;
    }
  }

  return cleaned;
}

PacketCacheStatistics PacketCache::GetStatistics() const {
  return stats_;
}

void PacketCache::SetOnPacketLostCallback(OnPacketLostCallback callback) {
  on_packet_lost_ = std::move(callback);
}

void PacketCache::SetOnPacketRecoveredCallback(OnPacketRecoveredCallback callback) {
  on_packet_recovered_ = std::move(callback);
}

void PacketCache::SetConfig(const PacketCacheConfig& config) {
  config_ = config;
}

bool PacketCache::IsOldPacket(uint16_t seq_num, int64_t current_time_ms) const {
  // If cache is empty, packet is not old
  if (cache_.empty()) {
    return false;
  }

  // Check if packet is already in cache (duplicate)
  if (cache_.find(seq_num) != cache_.end()) {
    return true;
  }

  // Check if packet is too old (before the oldest in cache)
  uint16_t oldest_seq = cache_.begin()->first;
  if (IsNewerSequenceNumber(oldest_seq, seq_num)) {
    int64_t time_diff = current_time_ms - last_update_time_ms_;
    if (time_diff > config_.max_age_ms) {
      return true;
    }
  }

  return false;
}

uint16_t PacketCache::GetExpectedSequenceNumber() const {
  if (cache_.empty()) {
    return 0;
  }

  // Return the sequence number after the highest one
  return static_cast<uint16_t>(highest_seq_num_ + 1);
}

void PacketCache::HandlePacketLost(uint16_t seq_num) {
  stats_.total_packets_lost++;
  
  if (on_packet_lost_) {
    on_packet_lost_(seq_num);
  }
}

void PacketCache::HandlePacketRecovered(std::shared_ptr<RtpPacket> packet) {
  if (packet) {
    stats_.total_packets_recovered++;
    
    if (on_packet_recovered_) {
      on_packet_recovered_(packet);
    }
  }
}

}  // namespace minirtc
