/**
 * @file test_network_emulator.h
 * @brief Network emulator for testing
 */

#ifndef MINIRTC_TEST_NETWORK_EMULATOR_H
#define MINIRTC_TEST_NETWORK_EMULATOR_H

#include <random>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <chrono>

namespace minirtc {
namespace test {

// ============================================================================
// Network Conditions
// ============================================================================

/// Network condition configuration
struct NetworkCondition {
  double packet_loss_rate = 0.0;      // 丢包率 0.0-1.0
  int latency_ms = 0;                  // 固定延迟
  int latency_jitter_ms = 0;           // 延迟抖动
  double corrupt_rate = 0.0;          // 包损坏率
  int bandwidth_kbps = 0;             // 带宽限制 (0=不限制)
  double burst_loss_probability = 0.0; // 突发丢包概率
  
  NetworkCondition() = default;
  
  NetworkCondition(double loss, int latency, int jitter = 0)
      : packet_loss_rate(loss), latency_ms(latency), latency_jitter_ms(jitter) {}
};

// ============================================================================
// Network Emulator
// ============================================================================

/// Network emulator for simulating network conditions
class NetworkEmulator {
 public:
  explicit NetworkEmulator(const NetworkCondition& condition);
  
  // 模拟网络传输，返回是否丢包
  bool ShouldDrop();
  
  // 添加延迟
  int64_t GetLatencyMs();
  
  // 可能损坏包
  bool ShouldCorrupt();
  
  // 重置状态
  void Reset();
  
  // 获取统计数据
  struct Stats {
    uint64_t total_packets = 0;
    uint64_t dropped_packets = 0;
    uint64_t corrupted_packets = 0;
    double avg_latency_ms = 0;
  };
  
  Stats GetStats() const;

 private:
  NetworkCondition condition_;
  std::mt19937 rng_;
  std::uniform_real_distribution<double> dist_;
  mutable std::mutex mutex_;
  
  // 统计
  uint64_t total_packets_ = 0;
  uint64_t dropped_packets_ = 0;
  uint64_t corrupted_packets_ = 0;
  std::vector<int64_t> latency_samples_;
};

// ============================================================================
// Delayed Packet (for latency simulation)
// ============================================================================

struct DelayedPacket {
  std::vector<uint8_t> data;
  int64_t delivery_time_ms;
  uint16_t seq;
};

// ============================================================================
// Packet Reorder Buffer
// ============================================================================

class PacketReorderBuffer {
 public:
  explicit PacketReorderBuffer(int buffer_ms = 50);
  
  // 添加包到缓冲区
  void AddPacket(uint16_t seq, std::function<void()> deliver_callback);
  
  // 检查是否有包可以输出
  bool HasPacket() const;
  
  // 获取下一个包
  std::function<void()> GetNextPacket();
  
  // 推进时间
  void AdvanceTime(int64_t ms);
  
 private:
  struct PendingPacket {
    uint16_t seq;
    int64_t deliver_time_ms;
    std::function<void()> callback;
  };
  
  int buffer_ms_;
  std::queue<PendingPacket> queue_;
  mutable std::mutex mutex_;
};

// ============================================================================
// Test Utilities
// ============================================================================

/// Generate test RTP payload
std::vector<uint8_t> GenerateTestPayload(size_t size);

/// Calculate packet loss percentage
double CalculateLossRate(uint64_t sent, uint64_t lost);

} // namespace test
} // namespace minirtc

#endif // MINIRTC_TEST_NETWORK_EMULATOR_H
