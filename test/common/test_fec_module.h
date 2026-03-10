/**
 * @file test_fec_module.h
 * @brief FEC module mock for testing
 */

#ifndef MINIRTC_TEST_FEC_MODULE_H
#define MINIRTC_TEST_FEC_MODULE_H

#include <vector>
#include <memory>
#include <cstdint>
#include <optional>

namespace minirtc {
namespace test {

// ============================================================================
// FEC Types
// ============================================================================

enum class FecType {
  kUlpFec,    // Unequal Level Protection FEC
  kXorFec,    // Simple XOR FEC
};

// FEC 配置
struct FecConfig {
  bool enable_fec = true;
  FecType fec_type = FecType::kUlpFec;
  int fec_percentage = 15;  // 冗余比例
  int fec_window_size = 8;  // 保护窗口大小
  int max_fec_packets = 4;  // 最大FEC包数量
};

// FEC 包
struct FecPacket {
  uint16_t seq;           // 保护的起始序列号
  uint8_t fec_level;      // FEC级别
  uint8_t payload_type;   // FEC负载类型
  std::vector<uint8_t> payload;  // FEC负载数据
};

// ============================================================================
// Mock FEC Module
// ============================================================================

class MockFecModule {
 public:
  explicit MockFecModule(const FecConfig& config);
  
  // FEC 编码
  // @param packets 原始RTP包
  // @return FEC包列表
  std::vector<FecPacket> Encode(const std::vector<uint16_t>& seq_nums,
                                 const std::vector<std::vector<uint8_t>>& payloads);
  
  // FEC 解码
  // @param packets 收到的RTP包
  // @param fec_packets 收到的FEC包
  // @return 恢复的负载数据 (如果有)
  struct DecodeResult {
    bool success;
    std::vector<std::vector<uint8_t>> recovered_payloads;
    uint32_t recovered_count;
  };
  
  DecodeResult Decode(const std::vector<uint16_t>& received_seqs,
                      const std::vector<std::vector<uint8_t>>& payloads,
                      const std::vector<FecPacket>& fec_packets);
  
  // 简化版: 对单帧进行FEC编码/解码
  bool EncodeFrame(const std::vector<std::vector<uint8_t>>& payloads,
                  std::vector<uint8_t>* fec_payload);
  
  bool DecodeFrame(const std::vector<std::vector<uint8_t>>& payloads,
                   const std::vector<uint8_t>& fec_payload,
                   std::vector<std::vector<uint8_t>>* recovered);
  
  // 获取配置
  FecConfig GetConfig() const;
  
  // 获取统计信息
  struct Stats {
    uint64_t frames_encoded = 0;
    uint64_t frames_decoded = 0;
    uint64_t packets_recovered = 0;
    uint64_t recovery_failed = 0;
    uint64_t fec_bytes_sent = 0;
    double avg_overhead = 0;
  };
  
  Stats GetStats() const;
  
  // 重置
  void Reset();

 private:
  // XOR FEC 编码
  std::vector<uint8_t> XorEncode(const std::vector<std::vector<uint8_t>>& payloads);
  
  // XOR FEC 解码
  std::optional<std::vector<std::vector<uint8_t>>> XorDecode(
      const std::vector<std::vector<uint8_t>>& payloads,
      const std::vector<uint8_t>& fec_payload);
  
  FecConfig config_;
  Stats stats_;
};

// ============================================================================
// FEC Test Result
// ============================================================================

struct FecTestResult {
  uint64_t total_packets = 0;
  uint64_t lost_packets = 0;
  uint64_t recovered_packets = 0;
  uint64_t fec_packets_sent = 0;
  double recovery_rate = 0.0;
  double bandwidth_overhead = 0.0;
};

} // namespace test
} // namespace minirtc

#endif // MINIRTC_TEST_FEC_MODULE_H
