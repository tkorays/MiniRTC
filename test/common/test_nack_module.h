/**
 * @file test_nack_module.h
 * @brief NACK module mock for testing
 */

#ifndef MINIRTC_TEST_NACK_MODULE_H
#define MINIRTC_TEST_NACK_MODULE_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <mutex>

namespace minirtc {
namespace test {

// ============================================================================
// NACK Configuration
// ============================================================================

struct NackConfig {
  bool enable_nack = true;
  bool enable_rtx = true;
  int max_retransmissions = 3;
  int rtt_estimate_ms = 100;
  int nack_timeout_ms = 1000;
  int max_nack_list_size = 250;
};

// ============================================================================
// NACK Module (Mock Implementation)
// ============================================================================

class MockNackModule {
 public:
  explicit MockNackModule(const NackConfig& config);
  
  // 处理接收到的RTP包
  void OnRtpPacketReceived(uint16_t seq, uint32_t timestamp);
  
  // 获取需要重传的包列表
  std::vector<uint16_t> GetNackList(uint64_t current_time_ms);
  
  // 处理RTX包
  void OnRtxPacketReceived(uint16_t original_seq);
  
  // 获取统计信息
  struct Stats {
    uint64_t packets_received = 0;
    uint64_t packets_lost = 0;
    uint64_t nack_requests_sent = 0;
    uint64_t packets_recovered = 0;
  };
  
  Stats GetStats() const;
  
  // 重置
  void Reset();
  
  // 设置丢包回调（模拟网络丢包）
  void SetPacketLossCallback(std::function<bool(uint16_t)> callback);

 private:
  // 检测丢包
  void DetectPacketLoss(uint16_t seq);
  
  // 更新NACK列表
  void UpdateNackList(uint64_t current_time_ms);
  
  // 移除已恢复的包
  void RemoveRecovered(uint16_t seq);
  
  NackConfig config_;
  
  // 接收状态
  uint16_t expected_seq_ = 0;
  bool first_packet_ = true;
  
  // 待NACK包: seq -> (timestamp, 重传次数)
  struct NackEntry {
    uint32_t timestamp;
    int retransmit_count;
    int64_t last_nack_time_ms;
  };
  std::unordered_map<uint16_t, NackEntry> nack_map_;
  
  // 已收到的包（用于去重和检测乱序）
  std::unordered_set<uint16_t> received_packets_;
  
  // RTX恢复的包
  std::unordered_set<uint16_t> recovered_packets_;
  
  // 统计
  Stats stats_;
  
  // 丢包回调
  std::function<bool(uint16_t)> packet_loss_callback_;
  
  mutable std::mutex mutex_;
};

// ============================================================================
// NACK Test Result
// ============================================================================

struct NackTestResult {
  uint64_t total_packets = 0;
  uint64_t lost_packets = 0;
  uint64_t recovered_packets = 0;
  uint64_t nack_requests = 0;
  double recovery_rate = 0.0;
};

} // namespace test
} // namespace minirtc

#endif // MINIRTC_TEST_NACK_MODULE_H
