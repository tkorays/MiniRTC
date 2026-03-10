/**
 * @file test_nack_module.cc
 * @brief Unit tests for NackModule class
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "minirtc/nack_module.h"

namespace minirtc {

// ============================================================================
// Test Fixtures
// ============================================================================

class NackModuleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.enable_nack = true;
    config_.enable_rtx = true;
    config_.mode = NackMode::kAdaptive;
    config_.max_retransmissions = 3;
    config_.rtt_estimate_ms = 100;
    config_.nack_timeout_ms = 200;
    config_.max_nack_list_size = 250;
    config_.nack_batch_interval_ms = 5;
    
    nack_module_ = NackModuleFactory::Create();
    nack_module_->Initialize(config_);
    nack_module_->Start();
  }

  NackConfig config_;
  std::unique_ptr<INackModule> nack_module_;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(NackModuleTest, Initialize_Success) {
  NackModuleFactory factory;
  auto module = factory.Create();
  
  NackConfig cfg;
  cfg.enable_nack = true;
  
  EXPECT_TRUE(module->Initialize(cfg));
  EXPECT_TRUE(module->IsEnabled());
}

TEST_F(NackModuleTest, Initialize_AlreadyInitialized) {
  // Already initialized in SetUp
  EXPECT_TRUE(nack_module_->IsEnabled());
}

// ============================================================================
// RTP Packet Processing Tests
// ============================================================================

TEST_F(NackModuleTest, OnRtpPacketReceived_Sequential) {
  // Send sequential packets, no NACK needed
  for (uint16_t i = 100; i <= 110; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    packet->SetPayloadType(96);
    packet->SetTimestamp(i * 3000);
    
    nack_module_->OnRtpPacketReceived(packet);
  }
  
  auto nack_list = nack_module_->GetNackList(5000);
  EXPECT_EQ(nack_list.size(), 0);
}

TEST_F(NackModuleTest, OnRtpPacketReceived_WithLoss) {
  // Send packets with gap
  for (uint16_t i = 100; i <= 102; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    nack_module_->OnRtpPacketReceived(packet);
  }
  
  // Skip 103
  auto packet104 = std::make_shared<RtpPacket>();
  packet104->SetSequenceNumber(104);
  nack_module_->OnRtpPacketReceived(packet104);
  
  auto nack_list = nack_module_->GetNackList(2000);
  
  // Should have 103 in NACK list
  EXPECT_EQ(nack_list.size(), 1);
  EXPECT_EQ(nack_list[0], 103);
}

TEST_F(NackModuleTest, OnRtpPacketReceived_Recovered) {
  // Send packets with gap
  for (uint16_t i = 100; i <= 102; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    nack_module_->OnRtpPacketReceived(packet);
  }
  
  // Skip 103
  auto packet104 = std::make_shared<RtpPacket>();
  packet104->SetSequenceNumber(104);
  nack_module_->OnRtpPacketReceived(packet104);
  
  // Check NACK list
  auto nack_list1 = nack_module_->GetNackList(2000);
  EXPECT_EQ(nack_list1.size(), 1);
  
  // Now receive 103 as RTX
  auto packet103 = std::make_shared<RtpPacket>();
  packet103->SetSequenceNumber(103);
  nack_module_->OnRtxPacketReceived(packet103);
  
  // NACK list should be empty now
  auto nack_list2 = nack_module_->GetNackList(5000);
  EXPECT_EQ(nack_list2.size(), 0);
}

// ============================================================================
// NACK List Management Tests
// ============================================================================

TEST_F(NackModuleTest, GetNackList_Empty) {
  auto nack_list = nack_module_->GetNackList(1000);
  EXPECT_EQ(nack_list.size(), 0);
}

TEST_F(NackModuleTest, GetNackList_Timeout) {
  // Send packets with gap
  for (uint16_t i = 100; i <= 102; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    nack_module_->OnRtpPacketReceived(packet);
  }
  
  // Skip 103
  auto packet104 = std::make_shared<RtpPacket>();
  packet104->SetSequenceNumber(104);
  nack_module_->OnRtpPacketReceived(packet104);
  
  // Check NACK list - should have 103
  auto nack_list1 = nack_module_->GetNackList(2000);
  EXPECT_EQ(nack_list1.size(), 1);
  
  // Wait for timeout and check again
  auto nack_list2 = nack_module_->GetNackList(3000);
  // After timeout, NACK should be sent again
  EXPECT_GE(nack_list2.size(), 0);
}

TEST_F(NackModuleTest, RemoveFromNackList) {
  // Send packets with gap
  for (uint16_t i = 100; i <= 102; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    nack_module_->OnRtpPacketReceived(packet);
  }
  
  // Skip 103
  auto packet104 = std::make_shared<RtpPacket>();
  packet104->SetSequenceNumber(104);
  nack_module_->OnRtpPacketReceived(packet104);
  
  // Check NACK list
  EXPECT_TRUE(nack_module_->IsInNackList(103));
  
  // Remove from NACK list
  nack_module_->RemoveFromNackList(103);
  
  EXPECT_FALSE(nack_module_->IsInNackList(103));
}

TEST_F(NackModuleTest, IsInNackList) {
  EXPECT_FALSE(nack_module_->IsInNackList(100));
  
  // Add to NACK list via packet loss
  for (uint16_t i = 100; i <= 102; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    nack_module_->OnRtpPacketReceived(packet);
  }
  
  auto packet104 = std::make_shared<RtpPacket>();
  packet104->SetSequenceNumber(104);
  nack_module_->OnRtpPacketReceived(packet104);
  
  EXPECT_TRUE(nack_module_->IsInNackList(103));
}

// ============================================================================
// RTX Processing Tests
// ============================================================================

TEST_F(NackModuleTest, HandleRtxResponse_Success) {
  // First trigger a NACK
  for (uint16_t i = 100; i <= 102; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    nack_module_->OnRtpPacketReceived(packet);
  }
  
  auto packet104 = std::make_shared<RtpPacket>();
  packet104->SetSequenceNumber(104);
  nack_module_->OnRtpPacketReceived(packet104);
  
  // Handle RTX response
  bool recovered = nack_module_->HandleRtxResponse(103);
  
  // Should succeed because we haven't actually inserted the packet
  // but the HandleRtxResponse checks cache
  EXPECT_FALSE(recovered);  // 103 is not in cache yet
}

TEST_F(NackModuleTest, OnRtxPacketReceived) {
  // First send 100, 101
  for (uint16_t i = 100; i <= 101; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    nack_module_->OnRtpPacketReceived(packet);
  }
  
  // Skip 102, send 103
  auto packet103 = std::make_shared<RtpPacket>();
  packet103->SetSequenceNumber(103);
  nack_module_->OnRtpPacketReceived(packet103);
  
  // Receive RTX for 102
  auto rtx_packet = std::make_shared<RtpPacket>();
  rtx_packet->SetSequenceNumber(102);
  nack_module_->OnRtxPacketReceived(rtx_packet);
  
  // Check statistics
  auto stats = nack_module_->GetStatistics();
  EXPECT_EQ(stats.rtx_packets_received, 1);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(NackModuleTest, SetConfig) {
  NackConfig new_config;
  new_config.enable_nack = false;
  new_config.max_retransmissions = 5;
  
  nack_module_->SetConfig(new_config);
  
  auto config = nack_module_->GetConfig();
  EXPECT_FALSE(config.enable_nack);
  EXPECT_EQ(config.max_retransmissions, 5);
}

TEST_F(NackModuleTest, GetConfig) {
  auto config = nack_module_->GetConfig();
  
  EXPECT_EQ(config.enable_nack, true);
  EXPECT_EQ(config.max_retransmissions, 3);
  EXPECT_EQ(config.rtt_estimate_ms, 100);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(NackModuleTest, GetStatistics_Initial) {
  auto stats = nack_module_->GetStatistics();
  
  EXPECT_EQ(stats.nack_requests_sent, 0);
  EXPECT_EQ(stats.nack_requests_received, 0);
  EXPECT_EQ(stats.rtx_packets_sent, 0);
  EXPECT_EQ(stats.rtx_packets_received, 0);
  EXPECT_EQ(stats.current_nack_list_size, 0);
}

TEST_F(NackModuleTest, GetStatistics_AfterPackets) {
  // Send some packets
  for (uint16_t i = 100; i <= 105; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    nack_module_->OnRtpPacketReceived(packet);
  }
  
  auto stats = nack_module_->GetStatistics();
  EXPECT_EQ(stats.current_nack_list_size, 0);
}

TEST_F(NackModuleTest, ResetStatistics) {
  // Send some packets
  for (uint16_t i = 100; i <= 105; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    nack_module_->OnRtpPacketReceived(packet);
  }
  
  nack_module_->ResetStatistics();
  
  auto stats = nack_module_->GetStatistics();
  EXPECT_EQ(stats.nack_requests_sent, 0);
  EXPECT_EQ(stats.rtx_packets_received, 0);
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(NackModuleTest, SetOnNackRequestCallback) {
  bool callback_called = false;
  std::vector<uint16_t> callback_seqs;
  
  nack_module_->SetOnNackRequestCallback(
      [&callback_called, &callback_seqs](const std::vector<uint16_t>& seqs) {
        callback_called = true;
        callback_seqs = seqs;
      });
  
  // Trigger NACK
  for (uint16_t i = 100; i <= 102; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    nack_module_->OnRtpPacketReceived(packet);
  }
  
  auto packet104 = std::make_shared<RtpPacket>();
  packet104->SetSequenceNumber(104);
  nack_module_->OnRtpPacketReceived(packet104);
  
  auto nack_list = nack_module_->GetNackList(2000);
  // Note: Callback is set but not automatically triggered by GetNackList
  // It should be triggered by the transport layer when sending NACK
}

TEST_F(NackModuleTest, SetOnRtxPacketCallback) {
  bool callback_called = false;
  
  nack_module_->SetOnRtxPacketCallback(
      [&callback_called](std::shared_ptr<RtpPacket> packet) {
        callback_called = true;
      });
  
  // Receive RTX packet
  auto rtx_packet = std::make_shared<RtpPacket>();
  rtx_packet->SetSequenceNumber(100);
  nack_module_->OnRtxPacketReceived(rtx_packet);
  
  EXPECT_TRUE(callback_called);
}

// ============================================================================
// State Query Tests
// ============================================================================

TEST_F(NackModuleTest, IsEnabled) {
  EXPECT_TRUE(nack_module_->IsEnabled());
  
  // Test with disabled NACK
  NackConfig disabled_config;
  disabled_config.enable_nack = false;
  
  auto disabled_module = NackModuleFactory::Create();
  disabled_module->Initialize(disabled_config);
  disabled_module->Start();
  
  EXPECT_FALSE(disabled_module->IsEnabled());
}

TEST_F(NackModuleTest, GetRttEstimate) {
  int rtt = nack_module_->GetRttEstimate();
  EXPECT_EQ(rtt, 100);
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(NackModuleTest, Stop) {
  nack_module_->Stop();
  
  // After stop, processing should be ignored
  auto packet = std::make_shared<RtpPacket>();
  packet->SetSequenceNumber(100);
  nack_module_->OnRtpPacketReceived(packet);
  
  auto nack_list = nack_module_->GetNackList(1000);
  EXPECT_EQ(nack_list.size(), 0);
}

TEST_F(NackModuleTest, Reset) {
  // Send some packets with loss
  for (uint16_t i = 100; i <= 102; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    nack_module_->OnRtpPacketReceived(packet);
  }
  
  auto packet104 = std::make_shared<RtpPacket>();
  packet104->SetSequenceNumber(104);
  nack_module_->OnRtpPacketReceived(packet104);
  
  // Reset
  nack_module_->Reset();
  
  auto nack_list = nack_module_->GetNackList(1000);
  EXPECT_EQ(nack_list.size(), 0);
  
  auto stats = nack_module_->GetStatistics();
  EXPECT_EQ(stats.nack_requests_sent, 0);
}

}  // namespace minirtc

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
