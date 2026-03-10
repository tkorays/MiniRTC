/**
 * @file packet_cache.h
 * @brief PacketCache class for NACK and FEC modules
 */

#ifndef MINIRTC_PACKET_CACHE_H
#define MINIRTC_PACKET_CACHE_H

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <map>
#include <functional>

#include "minirtc/transport/rtp_packet.h"

namespace minirtc {

// ============================================================================
// Configuration
// ============================================================================

/// Packet cache configuration
struct PacketCacheConfig {
  /// Maximum number of packets to cache
  size_t max_cache_size = 512;
  
  /// Maximum age of packets in milliseconds
  int64_t max_age_ms = 5000;
  
  /// Enable jitter estimation
  bool enable_jitter_estimation = true;
};

// ============================================================================
// PacketCache Item
// ============================================================================

/// Packet cache item containing packet and metadata
struct PacketCacheItem {
  std::shared_ptr<RtpPacket> packet;
  int64_t received_time_ms;
  bool is_retransmission;
  uint8_t ref_count;
  
  PacketCacheItem()
      : received_time_ms(0),
        is_retransmission(false),
        ref_count(0) {}
};

// ============================================================================
// PacketCache
// ============================================================================

/// Callback types
using OnPacketLostCallback = std::function<void(uint16_t seq_num)>;
using OnPacketRecoveredCallback = std::function<void(std::shared_ptr<RtpPacket> packet)>;

/// Statistics structure
struct PacketCacheStatistics {
  uint64_t total_packets_received;
  uint64_t total_packets_lost;
  uint64_t total_packets_recovered;
  uint32_t current_seq;
  int64_t last_packet_time_ms;
  
  PacketCacheStatistics()
      : total_packets_received(0),
        total_packets_lost(0),
        total_packets_recovered(0),
        current_seq(0),
        last_packet_time_ms(0) {}
};

/// Packet cache manager for NACK and FEC modules
class PacketCache {
 public:
  /// Constructor with config
  explicit PacketCache(const PacketCacheConfig& config);
  
  /// Destructor
  ~PacketCache() = default;

  // ========================================================================
  // Basic Operations
  // ========================================================================

  /// Insert a packet into the cache
  /// @param packet Packet to insert
  /// @return true if inserted successfully
  bool InsertPacket(std::shared_ptr<RtpPacket> packet);

  /// Get packet by sequence number
  /// @param seq_num Sequence number
  /// @return Packet pointer or nullptr if not found
  std::shared_ptr<RtpPacket> GetPacket(uint16_t seq_num);

  /// Check if packet exists
  /// @param seq_num Sequence number
  /// @return true if exists
  bool HasPacket(uint16_t seq_num) const;

  /// Remove packet from cache
  /// @param seq_num Sequence number
  /// @return true if removed
  bool RemovePacket(uint16_t seq_num);

  // ========================================================================
  // Loss Detection
  // ========================================================================

  /// Handle new packet arrival and detect lost packets
  /// @param seq_num Sequence number of arrived packet
  /// @param current_time_ms Current timestamp
  /// @return List of lost sequence numbers
  std::vector<uint16_t> OnPacketArrived(uint16_t seq_num, int64_t current_time_ms);

  // ========================================================================
  // Batch Operations
  // ========================================================================

  /// Get packets in range [start_seq, end_seq]
  /// @param start_seq Start sequence number
  /// @param end_seq End sequence number
  /// @return List of packets (sorted by sequence number)
  std::vector<std::shared_ptr<RtpPacket>> GetPacketsInRange(
      uint16_t start_seq, uint16_t end_seq) const;

  /// Get all cached packets
  /// @return All packets
  std::vector<std::shared_ptr<RtpPacket>> GetAllPackets() const;

  // ========================================================================
  // State Management
  // ========================================================================

  /// Get current cache size
  size_t size() const { return cache_.size(); }

  /// Clear all cached packets
  void Clear();

  /// Cleanup expired packets
  /// @param current_time_ms Current timestamp
  /// @return Number of packets cleaned up
  size_t CleanupExpiredPackets(int64_t current_time_ms);

  // ========================================================================
  // Statistics
  // ========================================================================

  /// Get statistics
  PacketCacheStatistics GetStatistics() const;

  // ========================================================================
  // Callbacks
  // ========================================================================

  /// Set packet lost callback
  void SetOnPacketLostCallback(OnPacketLostCallback callback);
  
  /// Set packet recovered callback
  void SetOnPacketRecoveredCallback(OnPacketRecoveredCallback callback);

  // ========================================================================
  // Configuration
  // ========================================================================

  /// Update configuration
  void SetConfig(const PacketCacheConfig& config);

 private:
  /// Check if packet is old/duplicate
  bool IsOldPacket(uint16_t seq_num, int64_t current_time_ms) const;
  
  /// Get expected next sequence number
  uint16_t GetExpectedSequenceNumber() const;

  /// Handle packet loss
  void HandlePacketLost(uint16_t seq_num);
  
  /// Handle packet recovered
  void HandlePacketRecovered(std::shared_ptr<RtpPacket> packet);

  /// Configuration
  PacketCacheConfig config_;
  
  /// Cache storage: sequence_number -> PacketCacheItem
  std::map<uint16_t, PacketCacheItem> cache_;
  
  /// Statistics
  PacketCacheStatistics stats_;
  
  /// Callbacks
  OnPacketLostCallback on_packet_lost_;
  OnPacketRecoveredCallback on_packet_recovered_;
  
  /// Internal state
  uint16_t highest_seq_num_;
  int64_t last_update_time_ms_;
  bool initialized_;
};

}  // namespace minirtc

#endif  // MINIRTC_PACKET_CACHE_H
