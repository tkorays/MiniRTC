/**
 * @file test_nack_recovery.cc
 * @brief NACK packet loss recovery integration tests
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>
#include <vector>

#include "test_network_emulator.h"
#include "test_nack_module.h"

using namespace minirtc::test;

// ============================================================================
// NACK Recovery Test Fixture
// ============================================================================

class NackRecoveryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    nack_config_.enable_nack = true;
    nack_config_.enable_rtx = true;
    nack_config_.max_retransmissions = 3;
    nack_config_.rtt_estimate_ms = 50;
    nack_config_.nack_timeout_ms = 500;
  }
  
  NackConfig nack_config_;
  
  // 运行NACK测试的辅助函数
  NackTestResult RunNackTest(const NetworkCondition& condition, int packet_count);
};

NackTestResult NackRecoveryTest::RunNackTest(
    const NetworkCondition& condition, int packet_count) {
  
  NetworkEmulator emulator(condition);
  MockNackModule nack(nack_config_);
  
  NackTestResult result;
  result.total_packets = packet_count;
  
  for (int i = 0; i < packet_count; ++i) {
    uint16_t seq = 1000 + i;
    uint32_t timestamp = i * 3000;
    
    if (emulator.ShouldDrop()) {
      // 丢包：记录到NACK但不标记为已接收
      // NACK模块会在收到后续包时检测到丢包
      result.lost_packets++;
      continue;
    }
    
    // 正常接收
    nack.OnRtpPacketReceived(seq, timestamp);
  }
  
  // 等待NACK处理
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  
  // 获取NACK列表
  auto nack_list = nack.GetNackList(1000);
  
  // 模拟RTX响应
  for (uint16_t seq : nack_list) {
    nack.OnRtxPacketReceived(seq);
    result.recovered_packets++;
  }
  
  result.nack_requests = nack_list.size();
  
  // 计算恢复率
  if (result.lost_packets > 0) {
    result.recovery_rate = 
        static_cast<double>(result.recovered_packets) / result.lost_packets * 100.0;
  }
  
  return result;
}

// ============================================================================
// NACK Recovery Tests
// ============================================================================

TEST_F(NackRecoveryTest, RandomSingleLoss_1pct) {
  // 1%随机丢包
  NetworkCondition condition;
  condition.packet_loss_rate = 0.01;
  condition.latency_ms = 50;
  
  auto result = RunNackTest(condition, 1000);
  
  // 1%丢包应该能100%恢复
  EXPECT_GE(result.recovery_rate, 95.0);
  
  // 记录结果
  printf("1%%丢包: 总包=%lu, 丢包=%lu, 恢复=%lu, 恢复率=%.2f%%\n",
         result.total_packets, result.lost_packets, 
         result.recovered_packets, result.recovery_rate);
}

TEST_F(NackRecoveryTest, RandomSingleLoss_3pct) {
  // 3%随机丢包
  NetworkCondition condition;
  condition.packet_loss_rate = 0.03;
  condition.latency_ms = 50;
  
  auto result = RunNackTest(condition, 1000);
  
  // 3%丢包应该能高概率恢复
  EXPECT_GE(result.recovery_rate, 90.0);
}

TEST_F(NackRecoveryTest, RandomSingleLoss_5pct) {
  // 5%随机丢包
  NetworkCondition condition;
  condition.packet_loss_rate = 0.05;
  condition.latency_ms = 50;
  
  auto result = RunNackTest(condition, 500);
  
  // 5%丢包应该能大部分恢复
  EXPECT_GE(result.recovery_rate, 80.0);
}

TEST_F(NackRecoveryTest, RandomSingleLoss_10pct) {
  // 10%随机丢包
  NetworkCondition condition;
  condition.packet_loss_rate = 0.10;
  condition.latency_ms = 50;
  
  auto result = RunNackTest(condition, 300);
  
  // 10%丢包也应该能恢复一部分
  EXPECT_GE(result.recovery_rate, 60.0);
  
  printf("10%%丢包: 总包=%lu, 丢包=%lu, 恢复=%lu, 恢复率=%.2f%%\n",
         result.total_packets, result.lost_packets, 
         result.recovered_packets, result.recovery_rate);
}

TEST_F(NackRecoveryTest, BurstLoss_2Packets) {
  // 连续丢包测试
  NetworkCondition cond;
  cond.packet_loss_rate = 0.03;
  NetworkEmulator emulator(cond);
  MockNackModule nack(nack_config_);
  
  const int kPacketCount = 200;
  int lost_count = 0;
  bool burst_mode = true;
  int burst_remaining = 0;
  
  for (int i = 0; i < kPacketCount; ++i) {
    uint16_t seq = 2000 + i;
    uint32_t timestamp = i * 3000;
    
    // 模拟突发丢包: 每隔一段距离连续丢2个
    if (burst_remaining > 0) {
      lost_count++;
      burst_remaining--;
      continue;
    }
    
    if (emulator.ShouldDrop() && (i % 20 == 0)) {
      // 触发突发丢包
      burst_remaining = 1;  // 连续丢2个
      lost_count++;
      continue;
    }
    
    nack.OnRtpPacketReceived(seq, timestamp);
  }
  
  // NACK恢复
  auto nack_list = nack.GetNackList(1000);
  for (uint16_t seq : nack_list) {
    nack.OnRtxPacketReceived(seq);
  }
  
  auto stats = nack.GetStats();
  
  // 验证恢复率
  if (lost_count > 0) {
    double recovery = static_cast<double>(stats.packets_recovered) / lost_count * 100.0;
    EXPECT_GE(recovery, 80.0);
  }
}

TEST_F(NackRecoveryTest, RtxTimeout) {
  // RTX超时测试
  NetworkCondition condition;
  condition.packet_loss_rate = 0.1;
  condition.latency_ms = 100;
  
  NetworkEmulator emulator(condition);
  MockNackModule nack(nack_config_);
  
  const int kPacketCount = 100;
  
  // 发送所有包，部分丢包
  for (int i = 0; i < kPacketCount; ++i) {
    uint16_t seq = 3000 + i;
    
    if (emulator.ShouldDrop()) {
      // 丢包
      continue;
    }
    
    nack.OnRtpPacketReceived(seq, i * 3000);
  }
  
  // 获取NACK列表
  auto nack_list = nack.GetNackList(500);  // 立即获取
  
  // 第一次NACK请求
  EXPECT_GT(nack_list.size(), 0);
  
  // 等待超时
  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  
  // 再次获取NACK列表 - 应该仍然有超时未恢复的包
  auto nack_list2 = nack.GetNackList(1200);
  
  // 超时后应该发起重传请求
  auto stats = nack.GetStats();
  EXPECT_GT(stats.nack_requests_sent, 0);
}

TEST_F(NackRecoveryTest, ExcessiveLoss) {
  // 严重丢包场景测试
  NetworkCondition condition;
  condition.packet_loss_rate = 0.15;
  condition.latency_ms = 100;
  
  auto result = RunNackTest(condition, 200);
  
  // 15%丢包下，记录恢复率
  printf("严重丢包(15%%): 总包=%lu, 丢包=%lu, 恢复=%lu, 恢复率=%.2f%%\n",
         result.total_packets, result.lost_packets, 
         result.recovered_packets, result.recovery_rate);
  
  // 期望至少50%恢复
  EXPECT_GE(result.recovery_rate, 50.0);
}

TEST_F(NackRecoveryTest, NackDeduplication) {
  // NACK去重测试
  MockNackModule nack(nack_config_);
  
  const int kPacketCount = 50;
  
  // 丢包: seq=10, 11, 12
  for (int i = 0; i < kPacketCount; ++i) {
    uint16_t seq = 4000 + i;
    
    if (i >= 10 && i <= 12) {
      // 丢包，不调用OnRtpPacketReceived
      continue;
    }
    
    nack.OnRtpPacketReceived(seq, i * 3000);
  }
  
  // 多次请求NACK
  std::vector<uint16_t> nack_list1 = nack.GetNackList(100);
  std::vector<uint16_t> nack_list2 = nack.GetNackList(200);
  std::vector<uint16_t> nack_list3 = nack.GetNackList(300);
  
  // NACK列表应该相同（去重后）
  EXPECT_EQ(nack_list1.size(), nack_list2.size());
  EXPECT_EQ(nack_list2.size(), nack_list3.size());
  
  // 应该只有3个包需要重传
  EXPECT_EQ(nack_list1.size(), 3);
}

TEST_F(NackRecoveryTest, RttEstimation) {
  // RTT估计测试
  nack_config_.rtt_estimate_ms = 100;
  
  MockNackModule nack(nack_config_);
  
  // 模拟丢包和恢复
  for (int i = 0; i < 100; ++i) {
    uint16_t seq = 5000 + i;
    
    if (i == 50) {
      // 丢包
      continue;
    }
    
    nack.OnRtpPacketReceived(seq, i * 3000);
  }
  
  // 获取NACK请求
  auto nack_list = nack.GetNackList(100);
  
  if (!nack_list.empty()) {
    // 模拟RTX响应
    nack.OnRtxPacketReceived(nack_list[0]);
    
    // 验证恢复
    auto stats = nack.GetStats();
    EXPECT_GT(stats.packets_recovered, 0);
  }
}

TEST_F(NackRecoveryTest, HighLatencyNetwork) {
  // 高延迟网络下的NACK测试
  NetworkCondition condition;
  condition.packet_loss_rate = 0.03;
  condition.latency_ms = 200;
  condition.latency_jitter_ms = 50;
  
  auto result = RunNackTest(condition, 500);
  
  // 高延迟下也应该能恢复 (如果有丢包)
  if (result.lost_packets > 0) {
    EXPECT_GE(result.recovery_rate, 60.0);
  }
  
  printf("高延迟网络: 总包=%lu, 丢包=%lu, 恢复=%lu, 恢复率=%.2f%%\n",
         result.total_packets, result.lost_packets, 
         result.recovered_packets, result.recovery_rate);
}

// ============================================================================
// Main
// ============================================================================


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
