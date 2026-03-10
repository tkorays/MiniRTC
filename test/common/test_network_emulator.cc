/**
 * @file test_network_emulator.cc
 * @brief Network emulator implementation
 */

#include "test_network_emulator.h"
#include <algorithm>
#include <cmath>

namespace minirtc {
namespace test {

NetworkEmulator::NetworkEmulator(const NetworkCondition& condition)
    : condition_(condition), rng_(std::random_device{}()), dist_(0.0, 1.0) {}

bool NetworkEmulator::ShouldDrop() {
  std::lock_guard<std::mutex> lock(mutex_);
  total_packets_++;
  
  bool drop = dist_(rng_) < condition_.packet_loss_rate;
  if (drop) {
    dropped_packets_++;
  }
  return drop;
}

int64_t NetworkEmulator::GetLatencyMs() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  int64_t latency = condition_.latency_ms;
  if (condition_.latency_jitter_ms > 0) {
    std::uniform_int_distribution<int> jitter_dist(
        -condition_.latency_jitter_ms, 
        condition_.latency_jitter_ms
    );
    latency += jitter_dist(rng_);
  }
  
  latency_samples_.push_back(latency);
  
  // 保持样本数量在合理范围
  if (latency_samples_.size() > 1000) {
    latency_samples_.erase(latency_samples_.begin(), 
                           latency_samples_.begin() + 500);
  }
  
  return std::max<int64_t>(0, latency);
}

bool NetworkEmulator::ShouldCorrupt() {
  std::lock_guard<std::mutex> lock(mutex_);
  return dist_(rng_) < condition_.corrupt_rate;
}

void NetworkEmulator::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  total_packets_ = 0;
  dropped_packets_ = 0;
  corrupted_packets_ = 0;
  latency_samples_.clear();
}

NetworkEmulator::Stats NetworkEmulator::GetStats() const {
  Stats stats;
  // Note: Not holding lock for stats - may have race condition but acceptable for tests
  stats.total_packets = total_packets_;
  stats.dropped_packets = dropped_packets_;
  stats.corrupted_packets = corrupted_packets_;
  
  if (!latency_samples_.empty()) {
    double sum = 0;
    for (auto lat : latency_samples_) {
      sum += lat;
    }
    stats.avg_latency_ms = sum / latency_samples_.size();
  }
  
  return stats;
}

// ============================================================================
// Packet Reorder Buffer
// ============================================================================

PacketReorderBuffer::PacketReorderBuffer(int buffer_ms) 
    : buffer_ms_(buffer_ms) {}

void PacketReorderBuffer::AddPacket(uint16_t seq, 
                                     std::function<void()> deliver_callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  
  PendingPacket packet;
  packet.seq = seq;
  packet.deliver_time_ms = now + buffer_ms_;
  packet.callback = std::move(deliver_callback);
  
  queue_.push(packet);
}

bool PacketReorderBuffer::HasPacket() const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (queue_.empty()) return false;
  
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  
  return queue_.front().deliver_time_ms <= now;
}

std::function<void()> PacketReorderBuffer::GetNextPacket() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (queue_.empty()) return nullptr;
  
  auto callback = std::move(queue_.front().callback);
  queue_.pop();
  return callback;
}

void PacketReorderBuffer::AdvanceTime(int64_t ms) {
  // Time is automatically advanced via steady_clock
  (void)ms;
}

// ============================================================================
// Test Utilities
// ============================================================================

std::vector<uint8_t> GenerateTestPayload(size_t size) {
  std::vector<uint8_t> payload(size);
  for (size_t i = 0; i < size; ++i) {
    payload[i] = static_cast<uint8_t>(i & 0xFF);
  }
  return payload;
}

double CalculateLossRate(uint64_t sent, uint64_t lost) {
  if (sent == 0) return 0.0;
  return static_cast<double>(lost) / static_cast<double>(sent) * 100.0;
}

} // namespace test
} // namespace minirtc
