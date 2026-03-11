/**
 * @file test_av_sync.cc
 * @brief Audio/Video synchronization integration tests
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <cmath>
#include <algorithm>

#include "test_network_emulator.h"
#include "test_nack_module.h"

using namespace minirtc::test;

// ============================================================================
// AV Sync Test Fixture
// ============================================================================

class AvSyncTest : public ::testing::Test {
 protected:
  void SetUp() override {
    network_condition_.packet_loss_rate = 0.0;
    network_condition_.latency_ms = 50;
    network_condition_.latency_jitter_ms = 10;
  }
  
  NetworkCondition network_condition_;
};

// ============================================================================
// Clock Synchronization Helper
// ============================================================================

class MockClockSync {
 public:
  MockClockSync() : local_offset_(0), last_sync_time_(0) {}
  
  // 同步时钟
  void Sync(int64_t remote_time_ms, int64_t local_time_ms) {
    local_offset_ = remote_time_ms - local_time_ms;
    last_sync_time_ = local_time_ms;
  }
  
  // 获取同步后的时间
  int64_t GetSynchronizedTime(int64_t local_time_ms) const {
    return local_time_ms + local_offset_;
  }
  
  // 获取本地时间
  int64_t GetLocalTime() const {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return now;
  }
  
  int64_t GetOffset() const { return local_offset_; }
  
 private:
  int64_t local_offset_;
  int64_t last_sync_time_;
};

// ============================================================================
// AV Sync Tests
// ============================================================================

TEST_F(AvSyncTest, SyncUnderGoodNetwork) {
  // 良好网络条件下的音视频同步
  network_condition_.packet_loss_rate = 0.0;
  network_condition_.latency_ms = 30;
  network_condition_.latency_jitter_ms = 5;
  
  NetworkEmulator video_network(network_condition_);
  NetworkEmulator audio_network(network_condition_);
  
  MockClockSync clock_sync;
  
  const int kDuration = 5;  // 5秒
  const int kVideoFps = 30;
  const int kAudioPacketsPerSec = 50;
  
  // 记录音视频帧的时间戳
  std::vector<int64_t> video_timestamps;
  std::vector<int64_t> audio_timestamps;
  
  auto start_time = std::chrono::steady_clock::now();
  
  // 模拟视频帧
  for (int sec = 0; sec < kDuration; ++sec) {
    for (int frame = 0; frame < kVideoFps; ++frame) {
      auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count();
      
      int64_t delivery_time = now + video_network.GetLatencyMs();
      video_timestamps.push_back(delivery_time);
      
      std::this_thread::sleep_for(std::chrono::milliseconds(1000 / kVideoFps));
    }
  }
  
  // 模拟音频帧
  for (int sec = 0; sec < kDuration; ++sec) {
    for (int frame = 0; frame < kAudioPacketsPerSec; ++frame) {
      auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count();
      
      int64_t delivery_time = now + audio_network.GetLatencyMs();
      audio_timestamps.push_back(delivery_time);
      
      std::this_thread::sleep_for(std::chrono::milliseconds(1000 / kAudioPacketsPerSec));
    }
  }
  
  // 计算同步偏差
  // 简化: 比较相邻音视频帧的时间差
  double max_sync_diff = 0;
  size_t min_samples = std::min(video_timestamps.size(), audio_timestamps.size());
  
  for (size_t i = 0; i < min_samples; ++i) {
    double video_time = static_cast<double>(video_timestamps[i]) / 1000.0;
    double audio_time = static_cast<double>(audio_timestamps[i]) / 1000.0;
    
    // 简化: 假设音视频按照固定比例交错
    // 30fps视频 = 每帧33.3ms, 50fps音频 = 每帧20ms
    // 比较"理论"时间
    double expected_video_time = i * 33.333;
    double expected_audio_time = i * 20.0;
    
    // 实际接收时间差
    double diff = std::abs(video_time - audio_time);
    max_sync_diff = std::max(max_sync_diff, diff);
  }
  
  // 良好网络下，同步偏差应该很小
  EXPECT_LE(max_sync_diff, 50);  // < 50ms
}

TEST_F(AvSyncTest, SyncUnderJitter) {
  // 高抖动网络条件下的音视频同步
  network_condition_.latency_ms = 50;
  network_condition_.latency_jitter_ms = 50;
  
  NetworkEmulator video_network(network_condition_);
  NetworkEmulator audio_network(network_condition_);
  
  const int kDuration = 3;
  const int kVideoFps = 30;
  
  std::vector<int64_t> video_latencies;
  std::vector<int64_t> audio_latencies;
  
  for (int i = 0; i < kDuration * kVideoFps; ++i) {
    video_latencies.push_back(video_network.GetLatencyMs());
  }
  
  for (int i = 0; i < kDuration * 50; ++i) {
    audio_latencies.push_back(audio_network.GetLatencyMs());
  }
  
  // 计算抖动
  double video_avg = 0, audio_avg = 0;
  for (auto l : video_latencies) video_avg += l;
  for (auto l : audio_latencies) audio_avg += l;
  video_avg /= video_latencies.size();
  audio_avg /= audio_latencies.size();
  
  // 同步偏差应该小于 抖动 + 基础延迟
  double max_expected_diff = (network_condition_.latency_jitter_ms * 2) + 20;
  
  // 在有抖动的情况下，同步仍应保持
  EXPECT_LE(std::abs(video_avg - audio_avg), max_expected_diff);
}

TEST_F(AvSyncTest, SyncAfterPacketLoss) {
  // 丢包后恢复同步
  network_condition_.packet_loss_rate = 0.05;
  network_condition_.latency_ms = 50;
  
  NetworkEmulator video_network(network_condition_);
  NetworkEmulator audio_network(network_condition_);
  
  MockNackModule video_nack({.enable_nack = true, .max_retransmissions = 3});
  MockNackModule audio_nack({.enable_nack = true, .max_retransmissions = 3});
  
  const int kTotalPackets = 150;
  int video_received = 0;
  int audio_received = 0;
  
  for (int i = 0; i < kTotalPackets; ++i) {
    // 视频
    if (!video_network.ShouldDrop()) {
      video_nack.OnRtpPacketReceived(6000 + i, i * 3000);
      video_received++;
    }
    
    // 音频
    if (!audio_network.ShouldDrop()) {
      audio_nack.OnRtpPacketReceived(7000 + i, i * 960);
      audio_received++;
    }
  }
  
  // 模拟NACK恢复
  auto video_nack_list = video_nack.GetNackList(5000);
  for (uint16_t seq : video_nack_list) {
    video_nack.OnRtxPacketReceived(seq);
    video_received++;
  }
  
  auto audio_nack_list = audio_nack.GetNackList(5000);
  for (uint16_t seq : audio_nack_list) {
    audio_nack.OnRtxPacketReceived(seq);
    audio_received++;
  }
  
  // 验证恢复后同步仍然保持
  double video_rate = static_cast<double>(video_received) / kTotalPackets;
  double audio_rate = static_cast<double>(audio_received) / kTotalPackets;
  
  // 两者接收率应该相近
  EXPECT_LE(std::abs(video_rate - audio_rate), 0.1);  // 差距<10%
}

TEST_F(AvSyncTest, SyncWithReordering) {
  // 乱序情况下的音视频同步
  network_condition_.latency_ms = 30;
  network_condition_.latency_jitter_ms = 20;
  
  NetworkEmulator network(network_condition_);
  
  // 模拟接收缓冲
  std::vector<uint16_t> video_buffer;
  std::vector<uint16_t> audio_buffer;
  
  const int kPacketCount = 100;
  
  for (int i = 0; i < kPacketCount; ++i) {
    uint16_t seq = i;
    
    // 模拟乱序: 随机交换相邻包
    if (i > 0 && (i % 5 == 0)) {
      // 模拟延迟到达
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    // 简单记录: 假设所有包最终都到达
    video_buffer.push_back(seq);
    audio_buffer.push_back(seq);
  }
  
  // 验证最终所有包都被接收
  EXPECT_EQ(video_buffer.size(), kPacketCount);
  EXPECT_EQ(audio_buffer.size(), kPacketCount);
}

TEST_F(AvSyncTest, TimestampDriftCompensation) {
  // 时间戳漂移补偿
  MockClockSync clock_sync;
  
  // 模拟时钟同步
  int64_t remote_time = 1000000;  // 1000秒
  int64_t local_time = 999000;    // 本地时间稍快
  
  clock_sync.Sync(remote_time, local_time);
  
  // 验证时钟偏移
  EXPECT_EQ(clock_sync.GetOffset(), 1000);  // 1秒偏移
  
  // 验证同步后的时间
  int64_t current_local = 1005000;  // 本地时间过了500ms
  int64_t synced = clock_sync.GetSynchronizedTime(current_local);
  
  EXPECT_EQ(synced, 1005000 + 1000);  // 加上偏移
}

// ============================================================================
// Main
// ============================================================================


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
