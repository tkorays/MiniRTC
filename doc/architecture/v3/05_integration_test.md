# MiniRTC v2 集成测试方案

**版本**: 1.0  
**日期**: 2026-03-10  
**状态**: 初稿

---

## 1. 概述

本文档定义 MiniRTC v2 架构的集成测试方案，覆盖核心模块的单元测试、NACK 丢包重传、FEC 前向纠错、音视频同步等关键场景。

### 1.1 测试目标

- 验证 NACK 模块丢包检测与重传机制的正确性
- 验证 FEC 模块前向纠错在丢包场景下的恢复能力
- 验证音视频同步机制在网络抖动下的稳定性
- 验证本地双端通话的端到端流程
- 确保核心模块达到预设的覆盖率目标

### 1.2 测试环境

| 项目 | 要求 |
|------|------|
| 操作系统 | macOS 12+ / Ubuntu 20.04+ |
| 编译器 | GCC 10+ / Clang 14+ |
| 依赖 | Google Test, CMake 3.16+ |
| 网络模拟 | tc/netem (Linux), Network Link Conditioner (macOS) |

---

## 2. 单元测试框架设计

### 2.1 框架选型

采用 **Google Test (gtest)** 作为 C++ 单元测试框架，原因如下：

- 成熟的 C++ 测试框架，社区活跃
- 支持参数化测试、死亡测试、Mock 对象
- 与 CMake 构建系统无缝集成

### 2.2 项目结构

```
MiniRTC/
├── CMakeLists.txt
├── test/
│   ├── CMakeLists.txt
│   ├── common/
│   │   ├── mock_*.h          # Mock 类定义
│   │   ├── test_utils.h      # 测试工具函数
│   │   └── network_emulator.h # 网络模拟器
│   ├── unit/
│   │   ├── test_nack_module.cc
│   │   ├── test_fec_module.cc
│   │   ├── test_jitter_buffer.cc
│   │   ├── test_rtp_transport.cc
│   │   ├── test_clock_sync.cc
│   │   └── test_media_pipeline.cc
│   └── integration/
│       ├── test_local_loop.cc
│       ├── test_av_sync.cc
│       ├── test_nack_recovery.cc
│       └── test_fec_recovery.cc
```

### 2.3 CMake 配置

```cmake
# test/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(MiniRTCTests)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_subdirectory(unit)
add_subdirectory(integration)
```

### 2.4 基础测试工具

#### 2.4.1 Mock 类

```cpp
// test/common/mock_rtp_packet.h
#pragma once
#include <gmock/gmock.h>
#include <minirtc/rtp_packet.h>

namespace minirtc {
namespace test {

class MockRtpPacket : public RtpPacket {
 public:
  MOCK_METHOD(uint16_t, sequence_number, (), const override);
  MOCK_METHOD(uint32_t, timestamp, (), const override);
  MOCK_METHOD(uint8_t, payload_type, (), const override);
  MOCK_METHOD(const uint8_t*, payload, (), const override);
  MOCK_METHOD(size_t, payload_size, (), const override);
  
  // Helper 方法
  static std::shared_ptr<MockRtpPacket> Create(uint16_t seq, uint32_t ts);
};

} // namespace test
} // namespace minirtc
```

#### 2.4.2 网络模拟器

```cpp
// test/common/network_emulator.h
#pragma once
#include <functional>
#include <vector>
#include <random>

namespace minirtc {
namespace test {

struct NetworkCondition {
  double packet_loss_rate = 0.0;      // 丢包率 0.0-1.0
  int latency_ms = 0;                  // 固定延迟
  int latency_jitter_ms = 0;           // 延迟抖动
  double corrupt_rate = 0.0;           // 包损坏率
  int bandwidth_kbps = 0;              // 带宽限制 (0=不限制)
};

class NetworkEmulator {
 public:
  explicit NetworkEmulator(const NetworkCondition& condition);
  
  // 模拟网络传输，返回是否丢包
  bool ShouldDrop();
  
  // 添加延迟
  void ApplyLatency(int64_t* delivery_time_ms);
  
  // 可能损坏包
  bool ShouldCorrupt();
  
 private:
  NetworkCondition condition_;
  std::mt19937 rng_;
  std::uniform_real_distribution<double> dist_;
};

} // namespace test
} // namespace minirtc
```

---

## 3. 单元测试覆盖目标

### 3.1 NACK 模块测试用例

| 测试项 | 测试用例 | 验证点 |
|--------|----------|--------|
| 丢包检测 | `NackModuleTest.DetectSinglePacketLoss` | 连续序列号缺失时正确触发 NACK |
| 丢包检测 | `NackModuleTest.DetectMultiplePacketLoss` | 连续丢包正确识别 |
| 丢包检测 | `NackModuleTest.NoFalsePositiveOnReorder` | 乱序不触发误判 |
| NACK 生成 | `NackModuleTest.GenerateNackList` | NACK 列表正确生成 |
| NACK 去重 | `NackModuleTest.DeduplicateNackRequests` | 相同 seq 不重复生成 |
| 超时处理 | `NackModuleTest.NackTimeout` | 超过最大重传次数后移除 |
| RTX 处理 | `NackModuleTest.ProcessRtxPacket` | 重传包正确插入 |
| 配置更新 | `NackModuleTest.DynamicConfigUpdate` | 运行时配置生效 |

#### 示例测试代码

```cpp
// test/unit/test_nack_module.cc
#include <gtest/gtest.h>
#include <minirtc/nack_module.h>
#include "common/mock_rtp_packet.h"

namespace minirtc {
namespace test {

class NackModuleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.enable_nack = true;
    config_.enable_rtx = true;
    config_.max_retransmissions = 3;
    config_.rtt_estimate_ms = 100;
    nack_module_ = std::make_unique<NackModuleImpl>(config_);
  }
  
  NackConfig config_;
  std::unique_ptr<NackModule> nack_module_;
};

TEST_F(NackModuleTest, DetectSinglePacketLoss) {
  // 模拟接收 seq=1,2,4 (丢失 3)
  nack_module_->OnRtpPacketReceived(MockRtpPacket::Create(1, 1000));
  nack_module_->OnRtpPacketReceived(MockRtpPacket::Create(2, 1002));
  // seq=3 丢失
  nack_module_->OnRtpPacketReceived(MockRtpPacket::Create(4, 1004));
  
  auto nack_list = nack_module_->GetNackList(1100);
  ASSERT_EQ(nack_list.size(), 1);
  EXPECT_EQ(nack_list[0], 3);
}

TEST_F(NackModuleTest, DetectMultiplePacketLoss) {
  // 连续丢包测试
  nack_module_->OnRtpPacketReceived(MockRtpPacket::Create(1, 1000));
  nack_module_->OnRtpPacketReceived(MockRtpPacket::Create(2, 1002));
  nack_module_->OnRtpPacketReceived(MockRtpPacket::Create(5, 1008));
  
  auto nack_list = nack_module_->GetNackList(1100);
  ASSERT_EQ(nack_list.size(), 2);
  EXPECT_EQ(nack_list[0], 3);
  EXPECT_EQ(nack_list[1], 4);
}

} // namespace test
} // namespace minirtc
```

### 3.2 FEC 模块测试用例

| 测试项 | 测试用例 | 验证点 |
|--------|----------|--------|
| FEC 编码 | `FecModuleTest.EncodeSingleFrame` | 单帧 FEC 包生成 |
| FEC 编码 | `FecModuleTest.EncodeMultipleFrames` | 多帧 FEC 编码 |
| FEC 解码 | `FecModuleTest.RecoverSingleLoss` | 单丢包恢复 |
| FEC 解码 | `FecModuleTest.RecoverBurstLoss` | 连续丢包恢复 |
| FEC 解码 | `FecModuleTest.FailOnExcessiveLoss` | 超过冗余度时失败 |
| 配置生效 | `FecModuleTest.DynamicFecPercentage` | 冗余比例动态调整 |

#### 示例测试代码

```cpp
// test/unit/test_fec_module.cc
#include <gtest/gtest.h>
#include <minirtc/fec_module.h>
#include "common/mock_rtp_packet.h"

namespace minirtc {
namespace test {

class FecModuleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.enable_fec = true;
    config_.fec_type = FecType::kUlpFec;
    config_.fec_percentage = 10;
    fec_module_ = std::make_unique<FecModuleImpl>(config_);
  }
  
  FecConfig config_;
  std::unique_ptr<FecModule> fec_module_;
};

TEST_F(FecModuleTest, RecoverSingleLoss) {
  // 创建 10 个 RTP 包，丢失第 5 个
  std::vector<std::shared_ptr<RtpPacket>> packets;
  for (int i = 0; i < 10; ++i) {
    packets.push_back(MockRtpPacket::Create(i, 1000 + i * 20));
  }
  
  std::shared_ptr<RtpPacket> fec_packet;
  fec_module_->Encode(packets, &fec_packet);
  
  // 移除第 5 个包，模拟丢包
  packets.erase(packets.begin() + 4);
  
  std::shared_ptr<RtpPacket> recovered;
  bool success = fec_module_->Decode(packets, &recovered);
  
  EXPECT_TRUE(success);
  EXPECT_EQ(recovered->sequence_number(), 4);
}

} // namespace test
} // namespace minirtc
```

### 3.3 JitterBuffer 模块测试用例

| 测试项 | 测试用例 | 验证点 |
|--------|----------|--------|
| 包缓冲 | `JitterBufferTest.BufferPackets` | 乱序包正确缓冲 |
| 包排序 | `JitterBufferTest.SortBySeqNum` | 输出按序列号排序 |
| 帧输出 | `JitterBufferTest.FrameOutput` | 完整帧正确输出 |
| 延迟控制 | `JitterBufferTest.AdaptiveDelay` | 自适应延迟调整 |
| 溢出处理 | `JitterBufferTest.BufferOverflow` | 缓冲区满时正确丢弃 |

### 3.4 ClockSync 模块测试用例

| 测试项 | 测试用例 | 验证点 |
|--------|----------|--------|
| NTP 同步 | `ClockSyncTest.NtpSync` | NTP 时间同步 |
| RTT 计算 | `ClockSyncTest.RttCalculation` | RTT 正确计算 |
| 时钟漂移 | `ClockSyncTest.ClockDrift` | 本地时钟漂移补偿 |

### 3.5 MediaPipeline 模块测试用例

| 测试项 | 测试用例 | 验证点 |
|--------|----------|--------|
| 管道启动 | `MediaPipelineTest.StartStop` | 启动/停止流程 |
| 帧路由 | `MediaPipelineTest.FrameRouting` | 帧正确路由到目标 |
| 编码器切换 | `MediaPipelineTest.SwitchEncoder` | 动态切换编码器 |

---

## 4. 集成测试场景设计

### 4.1 本地双端测试 (Local Loop Test)

#### 4.1.1 测试目标

验证本地双端点之间音视频通话的完整流程，包括采集、编码、传输、解码、渲染全链路。

#### 4.1.2 测试架构

```
+-----------------+         +-----------------+
|   Endpoint A    |         |   Endpoint B    |
|                 |         |                 |
| +-------------+ |  RTP    | +-------------+ |
| |VideoCapture |-+---------+-►VideoRenderer| |
| +-------------+ |         | +-------------+ |
| +-------------+ |         | +-------------+ |
| |AudioCapture |-+---------+-►AudioPlayer  | |
| +-------------+ |         | +-------------+ |
| +-------------+ |  RTCP   | +-------------+ |
| |   Encoder   |-+---------+-►  Decoder    | |
| +-------------+ |         | +-------------+ |
+-----------------+         +-----------------+
```

#### 4.1.3 测试用例

| 测试项 | 测试用例 | 验证指标 |
|--------|----------|----------|
| 视频通话 | `LocalLoopTest.VideoCall_720p` | 帧率 >= 25fps, 延迟 < 300ms |
| 音频通话 | `LocalLoopTest.AudioCall` | 采样率 48kHz, 延迟 < 150ms |
| 混合通话 | `LocalLoopTest.AudioVideoCall` | 音视频独立工作 |
| 码率控制 | `LocalLoopTest.BitrateAdaptation` | 码率随网络动态调整 |
| 长时间运行 | `LocalLoopTest.Stability_10Min` | 10分钟无内存泄漏 |

#### 4.1.4 实现代码

```cpp
// test/integration/test_local_loop.cc
#include <gtest/gtest.h>
#include <minirtc/session_mgr.h>
#include <minirtc/media_pipeline.h>
#include "common/network_emulator.h"

namespace minirtc {
namespace test {

class LocalLoopTest : public ::testing::Test {
 protected:
  void SetUp() override {
    endpoint_a_ = std::make_unique<TestEndpoint>("A");
    endpoint_b_ = std::make_unique<TestEndpoint>("B");
    endpoint_a_->Connect(endpoint_b_.get());
  }
  
  std::unique_ptr<TestEndpoint> endpoint_a_;
  std::unique_ptr<TestEndpoint> endpoint_b_;
};

TEST_F(LocalLoopTest, VideoCall_720p) {
  endpoint_a_->StartVideo(1280, 720, 30);
  endpoint_b_->StartVideo(1280, 720, 30);
  
  // 运行 5 秒
  std::this_thread::sleep_for(std::chrono::seconds(5));
  
  auto stats_a = endpoint_b_->GetReceiveStats();
  auto stats_b = endpoint_a_->GetReceiveStats();
  
  // 验证接收端统计
  EXPECT_GE(stats_a.avg_fps, 25.0);
  EXPECT_LE(stats_a.avg_latency_ms, 300);
  EXPECT_GE(stats_b.avg_fps, 25.0);
}

TEST_F(LocalLoopTest, AudioVideoCall) {
  endpoint_a_->StartAudio(48000, 1);
  endpoint_b_->StartAudio(48000, 1);
  endpoint_a_->StartVideo(640, 480, 30);
  endpoint_b_->StartVideo(640, 480, 30);
  
  std::this_thread::sleep_for(std::chrono::seconds(5));
  
  // 验证音视频均正常
  auto audio_stats = endpoint_b_->GetAudioStats();
  auto video_stats = endpoint_b_->GetVideoStats();
  
  EXPECT_TRUE(audio_stats.is_active);
  EXPECT_TRUE(video_stats.is_active);
  EXPECT_GE(video_stats.frame_count, 100);
}

} // namespace test
} // namespace minirtc
```

---

### 4.2 音视频同步测试

#### 4.2.1 测试目标

验证音视频同步机制在网络抖动、丢包等场景下的表现，确保 A/V 同步误差在可接受范围内。

#### 4.2.2 测试指标

| 指标 | 目标值 | 说明 |
|------|--------|------|
| A/V 同步偏差 | ±50ms | 音频与视频时间差 |
| 视频帧率稳定性 | ≥ 95% | 目标帧率维持比例 |
| 音频采样稳定性 | ≥ 99% | 采样点丢失比例 |

#### 4.2.3 测试用例

| 测试项 | 网络条件 | 验证点 |
|--------|----------|--------|
| 基础同步 | 良好网络 (0% 丢包) | A/V 同步偏差 < 20ms |
| 抖动同步 | 延迟抖动 ±50ms | A/V 同步偏差 < 50ms |
| 丢包同步 | 5% 丢包率 | A/V 同步偏差 < 80ms |
| 恢复同步 | 丢包恢复后 | 3秒内恢复同步 |

#### 4.2.4 实现代码

```cpp
// test/integration/test_av_sync.cc
#include <gtest/gtest.h>
#include <minirtc/clock_sync.h>
#include "common/network_emulator.h"

namespace minirtc {
namespace test {

class AvSyncTest : public ::testing::Test {
 protected:
  void SetUp() override {
    network_.reset(new NetworkEmulator({.latency_ms = 50, .latency_jitter_ms = 30}));
    clock_sync_ = std::make_unique<ClockSync>();
  }
  
  std::unique_ptr<NetworkEmulator> network_;
  std::unique_ptr<ClockSync> clock_sync_;
};

TEST_F(AvSyncTest, SyncUnderGoodNetwork) {
  // 良好网络：无丢包，低延迟
  NetworkEmulator good_network({.packet_loss_rate = 0.0, .latency_ms = 30});
  
  auto av_sync = MeasureAvSync(good_network, 10);
  
  EXPECT_LE(std::abs(av_sync), 20);  // < 20ms
}

TEST_F(AvSyncTest, SyncUnderJitter) {
  // 高抖动网络
  NetworkEmulator jitter_network({
    .latency_ms = 50,
    .latency_jitter_ms = 50
  });
  
  auto av_sync = MeasureAvSync(jitter_network, 10);
  
  EXPECT_LE(std::abs(av_sync), 50);  // < 50ms
}

TEST_F(AvSyncTest, SyncAfterPacketLoss) {
  // 先丢包，后恢复
  NetworkEmulator lossy_network({
    .packet_loss_rate = 0.05,
    .latency_ms = 50
  });
  
  auto av_sync = MeasureAvSync(lossy_network, 15);
  
  EXPECT_LE(std::abs(av_sync), 80);  // < 80ms
  
  // 验证恢复时间
  auto recovery_time = MeasureSyncRecoveryTime(lossy_network);
  EXPECT_LE(recovery_time, 3000);  // 3秒内恢复
}

} // namespace test
} // namespace minirtc
```

---

### 4.3 NACK 丢包重传测试

#### 4.3.1 测试目标

验证 NACK 模块在各种丢包场景下的检测、请求、重传恢复能力。

#### 4.3.2 测试场景矩阵

| 场景 | 丢包率 | 丢包类型 | 预期恢复率 |
|------|--------|----------|------------|
| 随机丢包 | 1-3% | 随机单包 | 100% |
| 随机丢包 | 5% | 随机单包 | ≥ 95% |
| 突发丢包 | 3% | 连续 2-3 包 | ≥ 90% |
| 严重丢包 | 10% | 随机 | ≥ 70% |

#### 4.3.3 测试用例

| 测试项 | 测试用例 | 验证点 |
|--------|----------|--------|
| 随机单丢包 | `NackRecoveryTest.RandomSingleLoss_1pct` | 100% 恢复 |
| 随机单丢包 | `NackRecoveryTest.RandomSingleLoss_5pct` | ≥ 95% 恢复 |
| 突发丢包 | `NackRecoveryTest.BurstLoss_2Packets` | 连续 2 包恢复 |
| 突发丢包 | `NackRecoveryTest.BurstLoss_3Packets` | 连续 3 包恢复 |
| 重传超时 | `NackRecoveryTest.RtxTimeout` | 超时正确处理 |
| 过度丢包 | `NackRecoveryTest.ExcessiveLoss` | 10% 丢包下表现 |

#### 4.3.4 实现代码

```cpp
// test/integration/test_nack_recovery.cc
#include <gtest/gtest.h>
#include <minirtc/nack_module.h>
#include <minirtc/rtp_transport.h>
#include "common/network_emulator.h"

namespace minirtc {
namespace test {

struct NackTestResult {
  int total_packets = 0;
  int lost_packets = 0;
  int recovered_packets = 0;
  double recovery_rate = 0.0;
};

class NackRecoveryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    nack_config_.enable_nack = true;
    nack_config_.enable_rtx = true;
    nack_config_.max_retransmissions = 3;
    nack_config_.rtt_estimate_ms = 50;
    nack_module_ = std::make_unique<NackModuleImpl>(nack_config_);
  }
  
  NackConfig nack_config_;
  std::unique_ptr<NackModule> nack_module_;
  
  NackTestResult RunNackTest(const NetworkCondition& condition, int packet_count);
};

NackTestResult NackRecoveryTest::RunNackTest(
    const NetworkCondition& condition, int packet_count) {
  NetworkEmulator emulator(condition);
  NackTestResult result;
  result.total_packets = packet_count;
  
  for (int i = 0; i < packet_count; ++i) {
    auto packet = MockRtpPacket::Create(i, i * 20);
    
    if (emulator.ShouldDrop()) {
      result.lost_packets++;
      // 记录丢包，等待 NACK 触发
      continue;
    }
    
    nack_module_->OnRtpPacketReceived(packet);
  }
  
  // 等待 NACK 处理和重传
  auto nack_list = nack_module_->GetNackList(10000);
  
  // 模拟 RTX 响应
  for (uint16_t seq : nack_list) {
    auto rtx_packet = MockRtpPacket::Create(seq, seq * 20);
    nack_module_->OnRtxPacketReceived(rtx_packet);
    result.recovered_packets++;
  }
  
  if (result.lost_packets > 0) {
    result.recovery_rate = 
        (double)result.recovered_packets / result.lost_packets * 100.0;
  }
  
  return result;
}

TEST_F(NackRecoveryTest, RandomSingleLoss_1pct) {
  NetworkCondition condition{.packet_loss_rate = 0.01};
  
  auto result = RunNackTest(condition, 1000);
  
  EXPECT_GE(result.recovery_rate, 100.0);  // 100% 恢复
}

TEST_F(NackRecoveryTest, RandomSingleLoss_5pct) {
  NetworkCondition condition{.packet_loss_rate = 0.05};
  
  auto result = RunNackTest(condition, 1000);
  
  EXPECT_GE(result.recovery_rate, 95.0);  // >= 95%
}

TEST_F(NackRecoveryTest, BurstLoss_2Packets) {
  // 模拟突发丢包
  NetworkCondition condition;
  condition.packet_loss_rate = 0.03;
  // 通过配置 burst 参数模拟连续丢包
  
  auto result = RunNackTest(condition, 500);
  
  EXPECT_GE(result.recovery_rate, 90.0);
}

TEST_F(NackRecoveryTest, ExcessiveLoss) {
  NetworkCondition condition{.packet_loss_rate = 0.10};
  
  auto result = RunNackTest(condition, 500);
  
  EXPECT_GE(result.recovery_rate, 70.0);
  // 记录严重丢包下的表现
  LOG(INFO) << "Excessive loss recovery rate: " << result.recovery_rate << "%";
}

} // namespace test
} // namespace minirtc
```

---

### 4.4 FEC 前向纠错测试

#### 4.4.1 测试目标

验证 FEC 模块在不同丢包率和丢包模式下的前向纠错能力。

#### 4.4.2 FEC 参数配置

| 参数 | 默认值 | 说明 |
|------|--------|------|
| FEC 冗余比例 | 10-20% | 冗余包占比 |
| FEC 保护窗口 | 8 帧 | 保护帧数 |
| FEC 类型 | ULP-FEC | 层级保护 |

#### 4.4.3 测试场景矩阵

| 场景 | 丢包率 | FEC 冗余 | 预期恢复率 |
|------|--------|----------|------------|
| 低丢包 | 3% | 10% | 100% |
| 中丢包 | 5% | 15% | 100% |
| 高丢包 | 10% | 20% | ≥ 90% |
| 严重丢包 | 15% | 25% | ≥ 80% |

#### 4.4.4 测试用例

| 测试项 | 测试用例 | 验证点 |
|--------|----------|--------|
| 低丢包恢复 | `FecRecoveryTest.LowLoss_3pct` | 100% 恢复 |
| 中丢包恢复 | `FecRecoveryTest.MediumLoss_5pct` | 100% 恢复 |
| 高丢包恢复 | `FecRecoveryTest.HighLoss_10pct` | ≥ 90% 恢复 |
| 突发丢包 | `FecRecoveryTest.BurstLoss` | 连续丢包恢复 |
| FEC 开销 | `FecRecoveryTest.OverheadAnalysis` | 带宽开销测量 |
| FEC vs NACK | `FecRecoveryTest.ComparedWithNack` | 延迟对比 |

#### 4.4.5 实现代码

```cpp
// test/integration/test_fec_recovery.cc
#include <gtest/gtest.h>
#include <minirtc/fec_module.h>
#include <minirtc/nack_module.h>
#include "common/network_emulator.h"

namespace minirtc {
namespace test {

struct FecTestResult {
  int sent_packets = 0;
  int fec_packets = 0;
  int lost_packets = 0;
  int recovered_packets = 0;
  double recovery_rate = 0.0;
  int bandwidth_overhead = 0;  // 百分比
};

class FecRecoveryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fec_config_.enable_fec = true;
    fec_config_.fec_type = FecType::kUlpFec;
    fec_config_.fec_percentage = 15;
    fec_module_ = std::make_unique<FecModuleImpl>(fec_config_);
  }
  
  FecConfig fec_config_;
  std::unique_ptr<FecModule> fec_module_;
  
  FecTestResult RunFecTest(const NetworkCondition& condition, int frame_count);
};

FecTestResult FecRecoveryTest::RunFecTest(
    const NetworkCondition& condition, int frame_count) {
  NetworkEmulator emulator(condition);
  FecTestResult result;
  
  constexpr int packets_per_frame = 10;
  
  for (int f = 0; f < frame_count; ++f) {
    std::vector<std::shared_ptr<RtpPacket>> frame_packets;
    
    // 生成帧数据
    for (int i = 0; i < packets_per_frame; ++i) {
      frame_packets.push_back(
          MockRtpPacket::Create(f * packets_per_frame + i, f * 33)
      );
    }
    
    // FEC 编码
    std::shared_ptr<RtpPacket> fec_packet;
    fec_module_->Encode(frame_packets, &fec_packet);
    
    if (fec_packet) {
      result.fec_packets++;
    }
    
    // 模拟网络传输，丢包
    std::vector<std::shared_ptr<RtpPacket>> received;
    for (const auto& pkt : frame_packets) {
      if (!emulator.ShouldDrop()) {
        received.push_back(pkt);
      } else {
        result.lost_packets++;
      }
    }
    
    // 添加 FEC 包（通常不容易丢）
    if (fec_packet && !emulator.ShouldDrop()) {
      received.push_back(fec_packet);
    }
    
    // FEC 解码恢复
    std::shared_ptr<RtpPacket> recovered;
    if (fec_module_->Decode(received, &recovered)) {
      result.recovered_packets += (packets_per_frame - received.size());
    }
    
    result.sent_packets += packets_per_frame;
  }
  
  // 计算恢复率和带宽开销
  if (result.lost_packets > 0) {
    result.recovery_rate = 
        (double)result.recovered_packets / result.lost_packets * 100.0;
  }
  result.bandwidth_overhead = 
      (result.fec_packets * 100) / result.sent_packets;
  
  return result;
}

TEST_F(FecRecoveryTest, LowLoss_3pct) {
  NetworkCondition condition{.packet_loss_rate = 0.03};
  
  auto result = RunFecTest(condition, 100);
  
  EXPECT_GE(result.recovery_rate, 100.0);
  EXPECT_LE(result.bandwidth_overhead, 20);
}

TEST_F(FecRecoveryTest, MediumLoss_5pct) {
  NetworkCondition condition{.packet_loss_rate = 0.05};
  
  auto result = RunFecTest(condition, 100);
  
  EXPECT_GE(result.recovery_rate, 100.0);
}

TEST_F(FecRecoveryTest, HighLoss_10pct) {
  NetworkCondition condition{.packet_loss_rate = 0.10};
  
  auto result = RunFecTest(condition, 100);
  
  EXPECT_GE(result.recovery_rate, 90.0);
}

TEST_F(FecRecoveryTest, BandwidthOverhead) {
  NetworkCondition condition{.packet_loss_rate = 0.05};
  
  auto result = RunFecTest(condition, 200);
  
  // 验证 FEC 带宽开销在预期范围内
  EXPECT_LE(result.bandwidth_overhead, 25);
  LOG(INFO) << "FEC bandwidth overhead: " << result.bandwidth_overhead << "%";
}

TEST_F(FecRecoveryTest, ComparedWithNack) {
  // 对比 FEC 和 NACK 的恢复延迟
  NetworkCondition condition{.packet_loss_rate = 0.05, .latency_ms = 50};
  
  auto fec_result = RunFecTest(condition, 100);
  
  // NACK 需要 RTT 时间恢复，FEC 即时恢复
  // FEC 应该恢复更快（延迟更低）
  LOG(INFO) << "FEC recovery rate: " << fec_result.recovery_rate << "%";
}

} // namespace test
} // namespace minirtc
```

---

### 4.5 综合压力测试

| 测试项 | 时长 | 网络条件 | 验证点 |
|--------|------|----------|--------|
| 30分钟压力测试 | 30min | 3% 丢包 | 内存稳定，无崩溃 |
| 码率自适应测试 | 10min | 动态带宽 | 码率平滑调整 |
| 切换测试 | 5min | WiFi/4G 切换 | 无感知的会话保持 |

---

## 5. 测试报告模板

### 5.1 单元测试报告

```
================================================================================
                          MiniRTC 单元测试报告
================================================================================

测试日期: YYYY-MM-DD
测试环境: macOS 14.0 / Clang 15.0
构建版本: v2.0.0-rc1

--------------------------------------------------------------------------------
                              测试摘要
--------------------------------------------------------------------------------
总测试用例:     XXX
通过:          XXX (XX.X%)
失败:          XXX (XX.X%)
跳过:          XXX (XX.X%)
执行时间:      XX.X 秒

--------------------------------------------------------------------------------
                              模块覆盖率
--------------------------------------------------------------------------------
模块               | 行覆盖率 | 分支覆盖率 | 函数覆盖率
-------------------|----------|------------|-------------
NACK Module        |  XX%     |   XX%      |   XX%
FEC Module         |  XX%     |   XX%      |   XX%
JitterBuffer        |  XX%     |   XX%      |   XX%
ClockSync          |  XX%     |   XX%      |   XX%
MediaPipeline      |  XX%     |   XX%      |   XX%
-------------------|----------|------------|-------------
总计               |  XX%     |   XX%      |   XX%

--------------------------------------------------------------------------------
                              失败用例详情
--------------------------------------------------------------------------------
[失败用例 1]
  用例: NackModuleTest.XXX
  文件: test/unit/test_nack_module.cc:123
  错误: EXPECT_EQ(xxx, yyy)...
  详情: ...

================================================================================
```

### 5.2 集成测试报告

```
================================================================================
                         MiniRTC 集成测试报告
================================================================================

测试日期: YYYY-MM-DD
测试环境: Ubuntu 22.04 / 8Core / 16GB
网络模拟: tc/netem

--------------------------------------------------------------------------------
                              测试摘要
--------------------------------------------------------------------------------
场景                          | 执行次数 | 通过率 | 平均恢复率
------------------------------|----------|--------|-------------
本地双端测试 (720p)           |   10     | 100%   |    N/A
本地双端测试 (1080p)          |   10     | 100%   |    N/A
音视频同步 (良好网络)         |   10     | 100%   |   <20ms
音视频同步 (抖动网络)         |   10     | 100%   |   <50ms
NACK 丢包恢复 (1%)            |   50     | 100%   |  100%
NACK 丢包恢复 (5%)            |   50     | 100%   |   98%
NACK 丢包恢复 (10%)           |   50     |  96%   |   85%
FEC 丢包恢复 (5%)             |   50     | 100%   |  100%
FEC 丢包恢复 (10%)            |   50     | 100%   |   92%
FEC 丢包恢复 (15%)            |   50     |  96%   |   78%

--------------------------------------------------------------------------------
                              性能指标
--------------------------------------------------------------------------------
指标                     | 目标值   | 实测值   | 状态
-------------------------|----------|----------|-------
端到端延迟 (720p)       | < 300ms  |  XXXms   |  PASS
端到端延迟 (1080p)      | < 400ms  |  XXXms   |  PASS
音频延迟                | < 150ms  |  XXXms   |  PASS
A/V 同步偏差            | < 50ms   |  XXXms   |  PASS
FEC 带宽开销            | < 25%    |   XX%    |  PASS
内存增长 (30min)         | < 50MB   |  XXXMB   |  PASS

--------------------------------------------------------------------------------
                              失败详情
--------------------------------------------------------------------------------
[失败 1]
  用例: NackRecoveryTest.ExcessiveLoss
  日期: YYYY-MM-DD HH:MM
  环境: Ubuntu 22.04, 5% 丢包
  现象: 恢复率 68%，低于预期 70%
  原因: RTT 估计不准确
  建议: 调整 RTT 估计参数

================================================================================
```

---

## 6. 覆盖率目标

### 6.1 覆盖率指标定义

| 指标 | 定义 | 测量工具 |
|------|------|----------|
| 行覆盖率 | 已执行代码行数 / 总行数 | gcov / lcov |
| 分支覆盖率 | 已执行分支数 / 总分支数 | gcov / lcov |
| 函数覆盖率 | 已执行函数数 / 总函数数 | gcov / lcov |

### 6.2 覆盖率目标

| 模块 | 行覆盖率目标 | 分支覆盖率目标 | 函数覆盖率目标 |
|------|--------------|----------------|----------------|
| NACK Module | ≥ 90% | ≥ 85% | ≥ 95% |
| FEC Module | ≥ 90% | ≥ 85% | ≥ 95% |
| JitterBuffer | ≥ 85% | ≥ 80% | ≥ 90% |
| ClockSync | ≥ 85% | ≥ 80% | ≥ 90% |
| MediaPipeline | ≥ 80% | ≥ 75% | ≥ 85% |
| RTPTransport | ≥ 80% | ≥ 75% | ≥ 85% |
| **总计** | **≥ 85%** | **≥ 80%** | **≥ 90%** |

### 6.3 覆盖率报告生成

```bash
# 使用 lcov 生成覆盖率报告
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLECOVERAGE=ON ..
make -j$(nproc)
ctest --output-on-failure

# 生成覆盖率报告
lcov --capture --directory . --output-file coverage.info \
  --include "*/minirtc/*"
lcov --summary coverage.info

# 生成 HTML 报告
genhtml coverage.info --output-directory coverage_html
```

---

## 7. 附录

### 7.1 测试用例命名规范

- 单元测试: `{Module}Test.{TestCase}`
- 集成测试: `{Scenario}Test.{TestCase}`
- 性能测试: `{Module}PerfTest.{TestCase}`

### 7.2 常用网络模拟参数

| 场景 | 丢包率 | 延迟 | 抖动 | 带宽 |
|------|--------|------|------|------|
| 优质网络 | 0% | 20ms | 5ms | 无限制 |
| 一般网络 | 1% | 50ms | 10ms | 无限制 |
| 弱网 | 3% | 100ms | 30ms | 1Mbps |
| 严重弱网 | 5-10% | 200ms | 50ms | 500Kbps |

### 7.3 参考资料

- [Google Test 官方文档](https://google.github.io/googletest/)
- [RFC 4585 - RTP Extensions for NACK](https://tools.ietf.org/html/rfc4585)
- [RFC 5109 - RTP FEC](https://tools.ietf.org/html/rfc5109)
- [WebRTC 丢包恢复策略](https://webrtc.org/)

---

*文档结束*
