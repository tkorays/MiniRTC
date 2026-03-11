/**
 * @file memory_pool.h
 * @brief MiniRTC memory pool implementation
 */

#ifndef MINIRTC_MEMORY_POOL_H_
#define MINIRTC_MEMORY_POOL_H_

#include <memory>
#include <vector>
#include <cstddef>
#include "minirtc/transport/rtp_packet.h"

namespace minirtc {

// ============================================================================
// Object Pool Template
// ============================================================================

/// Object pool template for reusable objects
template<typename T>
class ObjectPool {
 public:
  /// Shared pointer type
  using Ptr = std::shared_ptr<T>;

  /// Constructor with initial size
  explicit ObjectPool(size_t initial_size = 64) : in_use_(0) {
    if (initial_size > 0) {
      ExpandPool(initial_size);
    }
  }

  /// Destructor
  ~ObjectPool() = default;

  /// Acquire an object from the pool
  Ptr Acquire() {
    if (available_.empty()) {
      // Expand pool if no available objects
      ExpandPool(pool_.size() > 0 ? pool_.size() : 64);
    }

    // Get an available object
    T* obj = available_.back();
    available_.pop_back();
    ++in_use_;

    return std::shared_ptr<T>(obj, [this](T* p) {
      this->Release(Ptr(p, [](T*) {}));
    });
  }

  /// Release an object back to the pool
  void Release(Ptr obj) {
    if (obj) {
      available_.push_back(obj.get());
      --in_use_;
    }
  }

  /// Reserve more objects in the pool
  void Reserve(size_t size) {
    if (size > pool_.size()) {
      ExpandPool(size - pool_.size());
    }
  }

  /// Get count of available objects
  size_t GetAvailableCount() const { return available_.size(); }

  /// Get count of objects in use
  size_t GetInUseCount() const { return in_use_; }

 private:
  /// Expand the pool by creating more objects
  void ExpandPool(size_t count) {
    size_t old_size = pool_.size();
    pool_.resize(old_size + count);

    for (size_t i = old_size; i < pool_.size(); ++i) {
      available_.push_back(&pool_[i]);
    }
  }

  std::vector<T> pool_;          ///< Storage for pooled objects
  std::vector<T*> available_;   ///< Stack of available object pointers
  size_t in_use_ = 0;           ///< Number of objects currently in use
};

// ============================================================================
// RTP Packet Pool
// ============================================================================

/// RTP packet pool for efficient packet allocation
class RtpPacketPool {
 public:
  /// Shared pointer type
  using Ptr = std::shared_ptr<RtpPacketPool>;

  /// Constructor with sizes
  RtpPacketPool(size_t initial_size = 256, size_t max_size = 4096)
      : max_size_(max_size), next_available_(0), in_use_(0) {
    // Pre-allocate initial pool
    pool_.reserve(max_size_);
    for (size_t i = 0; i < initial_size; ++i) {
      pool_.push_back(std::make_shared<RtpPacket>());
    }
    next_available_ = initial_size;
  }

  /// Destructor
  ~RtpPacketPool() = default;

  /// Allocate an RTP packet from the pool
  std::shared_ptr<RtpPacket> Allocate() {
    if (next_available_ < pool_.size()) {
      // Reuse existing packet
      auto packet = pool_[next_available_];
      ++next_available_;
      ++in_use_;
      packet->Reset();
      return packet;
    }

    // Expand pool if possible
    if (pool_.size() < max_size_) {
      size_t expand_size = std::min(pool_.size() * 2, max_size_) - pool_.size();
      pool_.reserve(pool_.size() + expand_size);
      for (size_t i = 0; i < expand_size; ++i) {
        pool_.push_back(std::make_shared<RtpPacket>());
      }
      auto packet = pool_[next_available_];
      ++next_available_;
      ++in_use_;
      packet->Reset();
      return packet;
    }

    // Pool exhausted, allocate new packet (should not happen in normal use)
    ++in_use_;
    return std::make_shared<RtpPacket>();
  }

  /// Release an RTP packet back to the pool
  void Release(std::shared_ptr<RtpPacket> packet) {
    if (!packet) {
      return;
    }

    --in_use_;

    // Reset the packet
    packet->Reset();

    // If there's room in the pool, add it back
    if (next_available_ > 0) {
      // Move the released packet to the end of available pool
      // and reuse the slot that was just freed
      pool_[next_available_ - 1] = packet;
    }
  }

  /// Get count of available packets
  size_t GetAvailableCount() const {
    return pool_.size() - next_available_ + (next_available_ == 0 ? pool_.size() : 0);
  }

  /// Get count of packets in use
  size_t GetInUseCount() const { return in_use_; }

 private:
  size_t max_size_;                              ///< Maximum pool size
  std::vector<std::shared_ptr<RtpPacket>> pool_;  ///< Pool storage
  size_t next_available_ = 0;                    ///< Next available index
  size_t in_use_ = 0;                            ///< Packets in use
};

// ============================================================================
// Factory Functions
// ============================================================================

/// Create RTP packet pool
inline std::shared_ptr<RtpPacketPool> CreateRtpPacketPool(
    size_t initial_size = 256,
    size_t max_size = 4096) {
  return std::make_shared<RtpPacketPool>(initial_size, max_size);
}

}  // namespace minirtc

#endif  // MINIRTC_MEMORY_POOL_H_
