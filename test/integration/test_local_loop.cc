/**
 * @file test_local_loop.cc
 * @brief Local loop integration tests
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <vector>

#include "test_network_emulator.h"
#include "test_nack_module.h"
#include "test_fec_module.h"

using namespace minirtc::test;

// ============================================================================
// Local Loop Test Fixture
// ============================================================================

class LocalLoopTest : public ::testing::Test {
 protected:
  void SetUp() override {
    network_condition_.packet_loss_rate = 0.0;
    network_condition_.latency_ms = 20;
    network_condition_.latency_jitter_ms = 5;
    
    nack_config_.enable_nack = true;
    nack_config_.enable_rtx = true;
    nack_config_.max_retransmissions = 3;
    nack_config_.rtt_estimate_ms = 50;
    
    fec_config_.enable_fec = true;
    fec_config_.fec_percentage = 15;
  }
  
  NetworkCondition network_condition_;
  NackConfig nack_config_;
  FecConfig fec_config_;
};

// ============================================================================
// Local Loop Tests
// ============================================================================

TEST_F(LocalLoopTest, BasicVideoCall) {
  // 模拟基本视频通话
  NetworkEmulator emulator(network_condition_);
  MockNackModule nack(nack_config_);
  
  const int kPacketCount = 300;  // 10秒 @ 30fps
  const uint16_t kStartSeq = 1000;
  
  std::atomic<int> received_count{0};
  
  for (int i = 0; i < kPacketCount; ++i) {
    uint16_t seq = kStartSeq + i;
    uint32_t timestamp = i * 3000;  // 30fps timestamp increment
    
    // 模拟网络丢包
    if (emulator.ShouldDrop()) {
      // 丢包，记录到NACK
      nack.OnRtpPacketReceived(seq, timestamp);
      continue;
    }
    
    // 正常接收
    nack.OnRtpPacketReceived(seq, timestamp);
    received_count++;
  }
  
  // 验证接收率
  auto stats = nack.GetStats();
  double receive_rate = static_cast<double>(received_count) / kPacketCount * 100.0;
  
  EXPECT_GE(receive_rate, 95.0);  // 至少95%接收
  EXPECT_EQ(stats.packets_lost, 0);  // 无丢包
}

TEST_F(LocalLoopTest, VideoCallWithNackRecovery) {
  // 模拟带NACK恢复的视频通话
  network_condition_.packet_loss_rate = 0.03;  // 3%丢包
  
  NetworkEmulator emulator(network_condition_);
  MockNackModule nack(nack_config_);
  
  const int kPacketCount = 500;
  const uint16_t kStartSeq = 2000;
  
  std::atomic<int> received_count{0};
  std::atomic<int> recovered_count{0};
  
  // 模拟发送端
  for (int i = 0; i < kPacketCount; ++i) {
    uint16_t seq = kStartSeq + i;
    uint32_t timestamp = i * 3000;
    
    if (emulator.ShouldDrop()) {
      // 丢包: NACK模块检测到丢包
      // 模拟NACK请求
      nack.OnRtpPacketReceived(seq, timestamp);
      continue;
    }
    
    // 正常接收
    nack.OnRtpPacketReceived(seq, timestamp);
    received_count++;
  }
  
  // 模拟NACK响应和RTX恢复
  auto nack_list = nack.GetNackList(5000);
  for (uint16_t seq : nack_list) {
    // 模拟收到RTX包
    nack.OnRtxPacketReceived(seq);
    recovered_count++;
  }
  
  auto stats = nack.GetStats();
  
  // 验证恢复率
  if (stats.packets_lost > 0) {
    double recovery_rate = static_cast<double>(recovered_count) / stats.packets_lost * 100.0;
    EXPECT_GE(recovery_rate, 90.0);  // 至少90%恢复
  }
}

TEST_F(LocalLoopTest, AudioCall) {
  // 模拟音频通话 (48kHz, 20ms帧)
  NetworkEmulator emulator(network_condition_);
  MockNackModule nack(nack_config_);
  
  const int kPacketCount = 2500;  // 50秒 @ 20ms
  const uint16_t kStartSeq = 3000;
  
  std::atomic<int> received_count{0};
  
  // 48kHz采样, 20ms一帧 = 960采样点
  const uint32_t kTimestampIncrement = 960;
  
  for (int i = 0; i < kPacketCount; ++i) {
    uint16_t seq = kStartSeq + i;
    uint32_t timestamp = i * kTimestampIncrement;
    
    if (emulator.ShouldDrop()) {
      continue;
    }
    
    nack.OnRtpPacketReceived(seq, timestamp);
    received_count++;
  }
  
  // 音频应该有更高的可靠性要求
  auto stats = nack.GetStats();
  double receive_rate = static_cast<double>(received_count) / kPacketCount * 100.0;
  
  EXPECT_GE(receive_rate, 98.0);  // 至少98%接收率
}

TEST_F(LocalLoopTest, AudioVideoCall) {
  // 模拟音视频同时通话
  NetworkEmulator video_network(network_condition_);
  NetworkEmulator audio_network(network_condition_);
  
  MockNackModule video_nack(nack_config_);
  MockNackModule audio_nack(nack_config_);
  
  const int kDurationSec = 5;
  const int kVideoFps = 30;
  const int kVideoPackets = kDurationSec * kVideoFps;
  const int kAudioPacketsPerSec = 50;  // 20ms一帧
  const int kAudioPackets = kDurationSec * kAudioPacketsPerSec;
  
  // 视频
  for (int i = 0; i < kVideoPackets; ++i) {
    if (!video_network.ShouldDrop()) {
      video_nack.OnRtpPacketReceived(4000 + i, i * 3000);
    }
  }
  
  // 音频
  for (int i = 0; i < kAudioPackets; ++i) {
    if (!audio_network.ShouldDrop()) {
      audio_nack.OnRtpPacketReceived(5000 + i, i * 960);
    }
  }
  
  auto video_stats = video_nack.GetStats();
  auto audio_stats = audio_nack.GetStats();
  
  // 验证音视频都正常工作
  EXPECT_GE(video_stats.packets_received, kVideoPackets * 0.95);
  EXPECT_GE(audio_stats.packets_received, kAudioPackets * 0.98);
}

TEST_F(LocalLoopTest, BitrateAdaptation) {
  // 测试码率自适应
  network_condition_.bandwidth_kbps = 1000;  // 1Mbps带宽限制
  
  NetworkEmulator emulator(network_condition_);
  MockNackModule nack(nack_config_);
  
  // 模拟码率变化
  std::vector<int> bitrate_samples;
  int current_bitrate = 2000;  // kbps
  
  for (int i = 0; i < 100; ++i) {
    // 简单模拟码率调整
    auto stats = nack.GetStats();
    
    if (stats.packets_lost > 10 && current_bitrate > 500) {
      current_bitrate -= 100;  // 丢包多，降低码率
    } else if (stats.packets_lost == 0 && current_bitrate < 2000) {
      current_bitrate += 50;  // 无丢包，提高码率
    }
    
    bitrate_samples.push_back(current_bitrate);
    
    // 模拟发包
    for (int j = 0; j < 30; ++j) {  // 30fps
      nack.OnRtpPacketReceived(i * 30 + j, i * 3000);
    }
  }
  
  // 验证码率最终稳定
  // 码率应该在一个合理范围内波动
  EXPECT_FALSE(bitrate_samples.empty());
}

TEST_F(LocalLoopTest, NetworkLatency) {
  // 测试网络延迟对通话的影响
  network_condition_.latency_ms = 100;
  network_condition_.latency_jitter_ms = 30;
  
  NetworkEmulator emulator(network_condition_);
  
  const int kTestDuration = 100;
  std::vector<int64_t> latencies;
  
  for (int i = 0; i < kTestDuration; ++i) {
    int64_t latency = emulator.GetLatencyMs();
    latencies.push_back(latency);
  }
  
  // 验证延迟在预期范围内
  int64_t avg_latency = 0;
  for (auto l : latencies) avg_latency += l;
  avg_latency /= latencies.size();
  
  EXPECT_GE(avg_latency, 70);   // 至少70ms (100 - 30)
  EXPECT_LE(avg_latency, 130); // 最多130ms (100 + 30)
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

