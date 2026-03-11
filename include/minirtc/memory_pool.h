/**
 * @file memory_pool.h
 * @brief MiniRTC memory pool implementation - Optimized version
 */

#ifndef MINIRTC_MEMORY_POOL_H_
#define MINIRTC_MEMORY_POOL_H_

#include <memory>
#include <vector>
#include <array>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <thread>
#include <cstring>
#include <algorithm>

#include "minirtc/transport/rtp_packet.h"

namespace minirtc {

// ============================================================================
// Compiler Hints for Performance
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
// Aligned Memory Allocation
// ============================================================================

/// Aligned memory buffer for zero-copy operations
class AlignedBuffer {
 public:
  /// Create aligned buffer
  static std::shared_ptr<AlignedBuffer> Create(size_t size, size_t alignment = 16) {
    return std::shared_ptr<AlignedBuffer>(new AlignedBuffer(size, alignment));
  }

  /// Destructor - aligned free
  ~AlignedBuffer() {
    if (data_) {
      ::free(data_);
    }
  }

  /// Get raw pointer
  uint8_t* data() { return data_; }
  const uint8_t* data() const { return data_; }

  /// Get size
  size_t size() const { return size_; }

  /// Get capacity
  size_t capacity() const { return capacity_; }

 private:
  explicit AlignedBuffer(size_t size, size_t alignment)
      : size_(size), capacity_(size) {
    // Align to cache line (64 bytes) or specified alignment
    alignment = std::max(alignment, size_t(64));
    if (posix_memalign(reinterpret_cast<void**>(&data_), alignment, capacity_) != 0) {
      data_ = nullptr;
      capacity_ = 0;
    }
  }

  uint8_t* data_ = nullptr;
  size_t size_ = 0;
  size_t capacity_ = 0;
};

// ============================================================================
// Lock-Free Object Pool (Thread-Safe)
// ============================================================================

/// Lock-free object pool using atomic operations
template<typename T>
class alignas(64) LockFreeObjectPool {
 public:
  using Ptr = std::shared_ptr<T>;

  explicit LockFreeObjectPool(size_t initial_size = 64)
      : capacity_(initial_size * 2) {
    // Pre-allocate
    objects_.resize(capacity_);
    for (size_t i = 0; i < capacity_; ++i) {
      objects_[i] = std::make_shared<T>();
    }
    available_count_.store(initial_size);
  }

  Ptr Acquire() {
    size_t current = available_count_.fetch_add(1);
    if (MINIRTC_UNLIKELY(current >= capacity_)) {
      available_count_.fetch_sub(1);
      return std::make_shared<T>();  // Fallback
    }
    return objects_[current];
  }

  void Release(Ptr obj) {
    size_t current = available_count_.fetch_sub(1);
    if (MINIRTC_UNLIKELY(current > capacity_)) {
      available_count_.fetch_add(1);
      return;
    }
    objects_[current - 1] = obj;
  }

  size_t GetInUseCount() const {
    return available_count_.load();
  }

 private:
  std::vector<Ptr> objects_;
  std::atomic<size_t> available_count_{0};
  size_t capacity_;
};

// ============================================================================
// Thread-Local Object Pool
// ============================================================================

/// Thread-local pool for reducing lock contention
template<typename T>
class ThreadLocalPool {
 public:
  using Ptr = std::shared_ptr<T>;

  ~ThreadLocalPool() {
    // Return local objects to global pool
    for (auto& obj : local_pool_) {
      if (global_pool_) {
        global_pool_->Release(obj);
      }
    }
  }

  void SetGlobalPool(std::shared_ptr<LockFreeObjectPool<T>> pool) {
    global_pool_ = pool;
  }

  Ptr Acquire() {
    if (!local_pool_.empty()) {
      Ptr obj = local_pool_.back();
      local_pool_.pop_back();
      return obj;
    }
    if (global_pool_) {
      return global_pool_->Acquire();
    }
    return std::make_shared<T>();
  }

  void Release(Ptr obj) {
    if (local_pool_.size() < local_capacity_) {
      local_pool_.push_back(obj);
    } else if (global_pool_) {
      global_pool_->Release(obj);
    }
  }

 private:
  static constexpr size_t local_capacity_ = 16;
  std::vector<Ptr> local_pool_;
  std::shared_ptr<LockFreeObjectPool<T>> global_pool_;
};

// ============================================================================
// Simple Object Pool (Original)
// ============================================================================

/// Object pool template for reusable objects
template<typename T>
class ObjectPool {
 public:
  using Ptr = std::shared_ptr<T>;

  explicit ObjectPool(size_t initial_size = 64) : in_use_(0) {
    if (initial_size > 0) {
      ExpandPool(initial_size);
    }
  }

  ~ObjectPool() = default;

  Ptr Acquire() {
    if (available_.empty()) {
      ExpandPool(pool_.size() > 0 ? pool_.size() : 64);
    }

    T* obj = available_.back();
    available_.pop_back();
    ++in_use_;

    return std::shared_ptr<T>(obj, [this](T* p) {
      this->Release(Ptr(p, [](T*) {}));
    });
  }

  void Release(Ptr obj) {
    if (obj) {
      available_.push_back(obj.get());
      --in_use_;
    }
  }

  void Reserve(size_t size) {
    if (size > pool_.size()) {
      ExpandPool(size - pool_.size());
    }
  }

  size_t GetAvailableCount() const { return available_.size(); }
  size_t GetInUseCount() const { return in_use_; }

 private:
  void ExpandPool(size_t count) {
    size_t old_size = pool_.size();
    pool_.resize(old_size + count);

    for (size_t i = old_size; i < pool_.size(); ++i) {
      available_.push_back(&pool_[i]);
    }
  }

  std::vector<T> pool_;
  std::vector<T*> available_;
  size_t in_use_ = 0;
};

// ============================================================================
// Optimized RTP Packet Pool
// ============================================================================

// Forward declaration
class RtpPacketPool;

/// Factory function declaration
inline std::shared_ptr<RtpPacketPool> CreateRtpPacketPool(
    size_t initial_size = 256,
    size_t max_size = 4096);

/// Optimized RTP packet pool with pre-allocated buffers
class RtpPacketPool {
 public:
  using Ptr = std::shared_ptr<RtpPacketPool>;

  RtpPacketPool(size_t initial_size = 256, size_t max_size = 4096)
      : max_size_(max_size), in_use_(0) {
    pool_.reserve(max_size_);
    
    // Pre-allocate packets with buffers
    for (size_t i = 0; i < initial_size; ++i) {
      auto packet = std::make_shared<RtpPacket>();
      // Pre-allocate buffer capacity to avoid reallocations
      packet->PreallocateBuffer();
      pool_.push_back(packet);
    }
    
    // Initialize available stack
    available_stack_.reserve(max_size_);
    for (size_t i = 0; i < pool_.size(); ++i) {
      available_stack_.push_back(pool_[i].get());
    }
    available_count_.store(pool_.size());
  }

  ~RtpPacketPool() = default;

  /// Allocate packet (fast path with atomic)
  std::shared_ptr<RtpPacket> Allocate() {
    // Fast path: try to get from available stack
    RtpPacket* packet = PopAvailable();
    
    if (MINIRTC_LIKELY(packet != nullptr)) {
      packet->Reset();
      in_use_.fetch_add(1, std::memory_order_relaxed);
      return std::shared_ptr<RtpPacket>(packet, [this](RtpPacket* p) {
        this->Deallocate(p);
      });
    }

    // Slow path: expand pool or allocate new
    return AllocateSlow();
  }

  /// Release packet back to pool
  void Release(std::shared_ptr<RtpPacket> packet) {
    if (MINIRTC_UNLIKELY(!packet)) {
      return;
    }

    packet->Reset();
    in_use_.fetch_sub(1, std::memory_order_relaxed);
    
    PushAvailable(packet.get());
  }

  size_t GetAvailableCount() const { 
    return available_count_.load(); 
  }

  size_t GetInUseCount() const { 
    return in_use_.load(std::memory_order_relaxed); 
  }

 private:
  RtpPacket* PopAvailable() {
    size_t old_val = available_count_.fetch_sub(1);
    if (old_val == 0) {
      available_count_.fetch_add(1);
      return nullptr;
    }
    return available_stack_[old_val - 1];
  }

  void PushAvailable(RtpPacket* packet) {
    size_t old_val = available_count_.fetch_add(1);
    available_stack_[old_val] = packet;
  }

  std::shared_ptr<RtpPacket> AllocateSlow() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Double-check after acquiring lock
    size_t old_available = available_count_.load(std::memory_order_relaxed);
    if (old_available > 0) {
      RtpPacket* packet = available_stack_[old_available - 1];
      available_count_.store(old_available - 1, std::memory_order_relaxed);
      packet->Reset();
      in_use_.fetch_add(1, std::memory_order_relaxed);
      return std::shared_ptr<RtpPacket>(packet, [this](RtpPacket* p) {
        this->Deallocate(p);
      });
    }

    // Expand pool
    if (pool_.size() < max_size_) {
      size_t expand_size = std::min(pool_.size() * 2, max_size_) - pool_.size();
      size_t old_size = pool_.size();
      
      pool_.reserve(pool_.size() + expand_size);
      available_stack_.reserve(pool_.size() + expand_size);
      
      for (size_t i = 0; i < expand_size; ++i) {
        auto packet = std::make_shared<RtpPacket>();
        packet->PreallocateBuffer();
        pool_.push_back(packet);
        available_stack_.push_back(packet.get());
      }
      
      available_count_.store(old_size + expand_size, std::memory_order_relaxed);
      
      RtpPacket* packet = available_stack_[old_size];
      available_count_.store(old_size + expand_size - 1, std::memory_order_relaxed);
      in_use_.fetch_add(1, std::memory_order_relaxed);
      
      return std::shared_ptr<RtpPacket>(packet, [this](RtpPacket* p) {
        this->Deallocate(p);
      });
    }

    // Pool exhausted - allocate new
    in_use_.fetch_add(1, std::memory_order_relaxed);
    auto packet = std::make_shared<RtpPacket>();
    packet->PreallocateBuffer();
    return packet;
  }

  void Deallocate(RtpPacket* packet) {
    if (!packet) return;
    
    // Find packet in pool - if found, return to available
    // Otherwise it's a newly allocated packet, just let it die
    for (size_t i = 0; i < pool_.size(); ++i) {
      if (pool_[i].get() == packet) {
        packet->Reset();
        PushAvailable(packet);
        in_use_.fetch_sub(1, std::memory_order_relaxed);
        return;
      }
    }
    
    // Not in pool - was dynamically allocated
    in_use_.fetch_sub(1, std::memory_order_relaxed);
  }

  size_t max_size_;
  std::atomic<size_t> in_use_{0};
  
  std::mutex mutex_;
  std::vector<std::shared_ptr<RtpPacket>> pool_;
  std::vector<RtpPacket*> available_stack_;
  std::atomic<size_t> available_count_{0};
};

// ============================================================================
// Buffer Pool for Network I/O
// ============================================================================

/// Buffer pool for network I/O operations (zero-copy)
class BufferPool {
 public:
  using Ptr = std::shared_ptr<BufferPool>;

  /// Buffer size options
  enum class BufferSize : size_t {
    Small = 256,
    Medium = 1024,
    Large = 4096,
    Jumbo = 8192
  };

  explicit BufferPool(size_t initial_count = 128)
      : default_size_(static_cast<size_t>(BufferSize::Medium)) {
    // Pre-allocate buffers of different sizes
    ExpandPool(BufferSize::Small, initial_count / 4);
    ExpandPool(BufferSize::Medium, initial_count / 2);
    ExpandPool(BufferSize::Large, initial_count / 4);
  }

  /// Allocate buffer (auto-selects size)
  std::shared_ptr<std::vector<uint8_t>> Allocate(size_t size = 0) {
    if (size <= static_cast<size_t>(BufferSize::Small)) {
      return AllocateFromPool(BufferSize::Small);
    } else if (size <= static_cast<size_t>(BufferSize::Medium)) {
      return AllocateFromPool(BufferSize::Medium);
    } else if (size <= static_cast<size_t>(BufferSize::Large)) {
      return AllocateFromPool(BufferSize::Large);
    } else {
      return AllocateFromPool(BufferSize::Jumbo);
    }
  }

  /// Release buffer
  void Release(std::shared_ptr<std::vector<uint8_t>> buffer) {
    if (!buffer) return;
    
    size_t capacity = buffer->capacity();
    buffer->clear();
    
    if (capacity <= static_cast<size_t>(BufferSize::Small)) {
      ReleaseToPool(BufferSize::Small, buffer);
    } else if (capacity <= static_cast<size_t>(BufferSize::Medium)) {
      ReleaseToPool(BufferSize::Medium, buffer);
    } else if (capacity <= static_cast<size_t>(BufferSize::Large)) {
      ReleaseToPool(BufferSize::Large, buffer);
    } else {
      ReleaseToPool(BufferSize::Jumbo, buffer);
    }
  }

 private:
  void ExpandPool(BufferSize size, size_t count) {
    auto& pool = pools_[static_cast<size_t>(size)];
    pool.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      auto buffer = std::make_shared<std::vector<uint8_t>>();
      buffer->reserve(static_cast<size_t>(size));
      pool.push_back(buffer);
    }
  }

  std::shared_ptr<std::vector<uint8_t>> AllocateFromPool(BufferSize size) {
    auto& pool = pools_[static_cast<size_t>(size)];
    if (!pool.empty()) {
      auto buffer = pool.back();
      pool.pop_back();
      buffer->clear();
      return buffer;
    }
    // Pool empty - allocate new
    auto buffer = std::make_shared<std::vector<uint8_t>>();
    buffer->reserve(static_cast<size_t>(size));
    return buffer;
  }

  void ReleaseToPool(BufferSize size, std::shared_ptr<std::vector<uint8_t>> buffer) {
    auto& pool = pools_[static_cast<size_t>(size)];
    if (pool.size() < max_pool_size_) {
      buffer->clear();
      pool.push_back(buffer);
    }
  }

  static constexpr size_t max_pool_size_ = 256;
  size_t default_size_;
  
  std::array<std::vector<std::shared_ptr<std::vector<uint8_t>>>, 4> pools_;
};

// ============================================================================
// Memory Pool Manager (Singleton)
// ============================================================================

/// Global memory pool manager
class MemoryPoolManager {
 public:
  static MemoryPoolManager& Instance() {
    static MemoryPoolManager instance;
    return instance;
  }

  /// Get RTP packet pool
  std::shared_ptr<RtpPacketPool> GetRtpPacketPool() {
    if (!rtp_packet_pool_) {
      rtp_packet_pool_ = std::make_shared<RtpPacketPool>(256, 4096);
    }
    return rtp_packet_pool_;
  }

  /// Get buffer pool
  std::shared_ptr<BufferPool> GetBufferPool() {
    if (!buffer_pool_) {
      buffer_pool_ = std::make_shared<BufferPool>(128);
    }
    return buffer_pool_;
  }

 private:
  MemoryPoolManager() = default;
  ~MemoryPoolManager() = default;
  
  MemoryPoolManager(const MemoryPoolManager&) = delete;
  MemoryPoolManager& operator=(const MemoryPoolManager&) = delete;

  std::shared_ptr<RtpPacketPool> rtp_packet_pool_;
  std::shared_ptr<BufferPool> buffer_pool_;
};

// ============================================================================
// Factory Functions
// ============================================================================

inline std::shared_ptr<RtpPacketPool> CreateRtpPacketPool(
    size_t initial_size,
    size_t max_size) {
  return std::make_shared<RtpPacketPool>(initial_size, max_size);
}

}  // namespace minirtc

#endif  // MINIRTC_MEMORY_POOL_H_
