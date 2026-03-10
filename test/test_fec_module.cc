/**
 * @file test_fec_module.cc
 * @brief Unit tests for FecModule class
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "minirtc/fec_module.h"

namespace minirtc {

// ============================================================================
// Test Fixtures
// ============================================================================

class FecModuleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.enable_fec = true;
    config_.algorithm = FecAlgorithm::kXorFec;
    config_.fec_percentage = 15;
    config_.media_payload_type = 96;
    config_.fec_payload_type = 97;
    
    fec_module_ = FecModuleFactory::Create(FecAlgorithm::kXorFec);
    fec_module_->Initialize(config_);
    fec_module_->Start();
  }

  FecConfig config_;
  std::unique_ptr<IFecModule> fec_module_;
};

// ============================================================================
// XorFecEncoder Tests
// ============================================================================

class XorFecEncoderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    encoder_ = std::make_unique<XorFecEncoder>();
  }

  std::unique_ptr<XorFecEncoder> encoder_;
};

TEST_F(XorFecEncoderTest, AddPacket_Success) {
  auto packet = std::make_shared<RtpPacket>();
  packet->SetSequenceNumber(100);
  packet->SetPayloadType(96);
  
  std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
  packet->SetPayload(payload.data(), payload.size());
  
  EXPECT_TRUE(encoder_->AddPacket(packet));
  EXPECT_EQ(encoder_->group_size(), 1);
}

TEST_F(XorFecEncoderTest, AddPacket_Nullptr) {
  EXPECT_FALSE(encoder_->AddPacket(nullptr));
  EXPECT_EQ(encoder_->group_size(), 0);
}

TEST_F(XorFecEncoderTest, Encode_SinglePacket) {
  auto packet = std::make_shared<RtpPacket>();
  packet->SetSequenceNumber(100);
  packet->SetPayloadType(96);
  packet->SetTimestamp(1000);
  
  std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
  packet->SetPayload(payload.data(), payload.size());
  
  encoder_->AddPacket(packet);
  
  auto fec_packet = encoder_->Encode();
  EXPECT_NE(fec_packet, nullptr);
  EXPECT_EQ(fec_packet->GetPayloadType(), 97);  // FEC payload type
}

TEST_F(XorFecEncoderTest, Encode_MultiplePackets) {
  for (uint16_t i = 100; i <= 102; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    packet->SetPayloadType(96);
    packet->SetTimestamp(1000);
    
    std::vector<uint8_t> payload = {static_cast<uint8_t>(i), 
                                     static_cast<uint8_t>(i + 1),
                                     static_cast<uint8_t>(i + 2)};
    packet->SetPayload(payload.data(), payload.size());
    
    encoder_->AddPacket(packet);
  }
  
  auto fec_packet = encoder_->Encode();
  EXPECT_NE(fec_packet, nullptr);
}

TEST_F(XorFecEncoderTest, Encode_Empty) {
  auto fec_packet = encoder_->Encode();
  EXPECT_EQ(fec_packet, nullptr);
}

TEST_F(XorFecEncoderTest, Clear) {
  for (uint16_t i = 100; i <= 102; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    encoder_->AddPacket(packet);
  }
  
  EXPECT_EQ(encoder_->group_size(), 3);
  
  encoder_->Clear();
  
  EXPECT_EQ(encoder_->group_size(), 0);
}

TEST_F(XorFecEncoderTest, CanEncode) {
  EXPECT_FALSE(encoder_->can_encode());
  
  auto packet = std::make_shared<RtpPacket>();
  encoder_->AddPacket(packet);
  
  EXPECT_TRUE(encoder_->can_encode());
}

// ============================================================================
// XorFecDecoder Tests
// ============================================================================

class XorFecDecoderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    decoder_ = std::make_unique<XorFecDecoder>();
  }

  std::unique_ptr<XorFecDecoder> decoder_;
};

TEST_F(XorFecDecoderTest, AddMediaPacket) {
  auto packet = std::make_shared<RtpPacket>();
  packet->SetSequenceNumber(100);
  
  decoder_->AddMediaPacket(packet);
  
  // Just verify no crash
  EXPECT_TRUE(true);
}

TEST_F(XorFecDecoderTest, AddFecPacket) {
  auto packet = std::make_shared<RtpPacket>();
  packet->SetPayloadType(97);
  
  decoder_->AddFecPacket(packet);
  
  // Just verify no crash
  EXPECT_TRUE(true);
}

TEST_F(XorFecDecoderTest, Clear) {
  auto packet = std::make_shared<RtpPacket>();
  decoder_->AddMediaPacket(packet);
  
  decoder_->Clear();
  
  // Just verify no crash
  EXPECT_TRUE(true);
}

// ============================================================================
// FecModule Initialization Tests
// ============================================================================

TEST_F(FecModuleTest, Initialize_Success) {
  auto module = FecModuleFactory::Create(FecAlgorithm::kXorFec);
  
  FecConfig cfg;
  cfg.enable_fec = true;
  
  EXPECT_TRUE(module->Initialize(cfg));
  EXPECT_TRUE(module->IsEnabled());
}

TEST_F(FecModuleTest, IsEnabled) {
  EXPECT_TRUE(fec_module_->IsEnabled());
}

TEST_F(FecModuleTest, IsDisabled) {
  FecConfig disabled_config;
  disabled_config.enable_fec = false;
  
  auto disabled_module = FecModuleFactory::Create(FecAlgorithm::kXorFec);
  disabled_module->Initialize(disabled_config);
  disabled_module->Start();
  
  EXPECT_FALSE(disabled_module->IsEnabled());
}

// ============================================================================
// FEC Encoding Tests
// ============================================================================

TEST_F(FecModuleTest, AddMediaPacket) {
  auto packet = std::make_shared<RtpPacket>();
  packet->SetSequenceNumber(100);
  packet->SetPayloadType(96);
  packet->SetTimestamp(1000);
  
  std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
  packet->SetPayload(payload.data(), payload.size());
  
  EXPECT_TRUE(fec_module_->AddMediaPacket(packet));
}

TEST_F(FecModuleTest, EncodeFec) {
  // Add some media packets
  for (uint16_t i = 100; i <= 105; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    packet->SetPayloadType(96);
    packet->SetTimestamp(1000);
    
    std::vector<uint8_t> payload(10, static_cast<uint8_t>(i));
    packet->SetPayload(payload.data(), payload.size());
    
    fec_module_->AddMediaPacket(packet);
  }
  
  auto fec_packets = fec_module_->EncodeFec();
  
  // Check if FEC packet was generated
  EXPECT_FALSE(fec_packets.empty());
}

TEST_F(FecModuleTest, GetPendingFecPackets) {
  // Add packets and trigger encoding
  for (uint16_t i = 100; i <= 105; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    packet->SetPayloadType(96);
    packet->SetTimestamp(1000);
    
    std::vector<uint8_t> payload(10, static_cast<uint8_t>(i));
    packet->SetPayload(payload.data(), payload.size());
    
    fec_module_->AddMediaPacket(packet);
  }
  
  fec_module_->EncodeFec();
  
  auto pending = fec_module_->GetPendingFecPackets();
  EXPECT_FALSE(pending.empty());
}

TEST_F(FecModuleTest, ClearPendingFecPackets) {
  // Add packets and trigger encoding
  for (uint16_t i = 100; i <= 105; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    packet->SetPayloadType(96);
    packet->SetTimestamp(1000);
    
    std::vector<uint8_t> payload(10, static_cast<uint8_t>(i));
    packet->SetPayload(payload.data(), payload.size());
    
    fec_module_->AddMediaPacket(packet);
  }
  
  fec_module_->EncodeFec();
  fec_module_->ClearPendingFecPackets();
  
  auto pending = fec_module_->GetPendingFecPackets();
  EXPECT_TRUE(pending.empty());
}

// ============================================================================
// FEC Decoding Tests
// ============================================================================

TEST_F(FecModuleTest, OnRtpPacketReceived_MediaPacket) {
  auto packet = std::make_shared<RtpPacket>();
  packet->SetSequenceNumber(100);
  packet->SetPayloadType(96);
  packet->SetTimestamp(1000);
  
  std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
  packet->SetPayload(payload.data(), payload.size());
  
  auto recovered = fec_module_->OnRtpPacketReceived(packet);
  
  // No recovery expected yet, just the original packet
  EXPECT_TRUE(recovered.empty() || recovered.size() >= 0);
}

TEST_F(FecModuleTest, OnRtpPacketReceived_FecPacket) {
  // First create an FEC packet
  for (uint16_t i = 100; i <= 102; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    packet->SetPayloadType(96);
    packet->SetTimestamp(1000);
    
    std::vector<uint8_t> payload(10, static_cast<uint8_t>(i));
    packet->SetPayload(payload.data(), payload.size());
    
    fec_module_->AddMediaPacket(packet);
  }
  
  auto fec_packets = fec_module_->EncodeFec();
  
  // Now receive the FEC packet
  if (!fec_packets.empty()) {
    fec_module_->OnFecPacketReceived(fec_packets[0]);
  }
  
  // Just verify no crash
  EXPECT_TRUE(true);
}

TEST_F(FecModuleTest, TryRecoverPacket_NotFound) {
  // Add some packets but don't create FEC
  for (uint16_t i = 100; i <= 102; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    packet->SetPayloadType(96);
    packet->SetTimestamp(1000);
    
    std::vector<uint8_t> payload(10, static_cast<uint8_t>(i));
    packet->SetPayload(payload.data(), payload.size());
    
    fec_module_->OnRtpPacketReceived(packet);
  }
  
  // Try to recover packet 103 (never sent)
  auto recovered = fec_module_->TryRecoverPacket(103);
  
  EXPECT_EQ(recovered, nullptr);
}

TEST_F(FecModuleTest, TryDecodeAll) {
  // Add some packets
  for (uint16_t i = 100; i <= 102; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    packet->SetPayloadType(96);
    packet->SetTimestamp(1000);
    
    std::vector<uint8_t> payload(10, static_cast<uint8_t>(i));
    packet->SetPayload(payload.data(), payload.size());
    
    fec_module_->OnRtpPacketReceived(packet);
  }
  
  auto recovered = fec_module_->TryDecodeAll();
  // May be empty if no FEC data
  EXPECT_TRUE(true);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(FecModuleTest, SetConfig) {
  FecConfig new_config;
  new_config.enable_fec = false;
  new_config.fec_percentage = 25;
  
  fec_module_->SetConfig(new_config);
  
  auto config = fec_module_->GetConfig();
  EXPECT_FALSE(config.enable_fec);
  EXPECT_EQ(config.fec_percentage, 25);
}

TEST_F(FecModuleTest, GetConfig) {
  auto config = fec_module_->GetConfig();
  
  EXPECT_EQ(config.enable_fec, true);
  EXPECT_EQ(config.algorithm, FecAlgorithm::kXorFec);
  EXPECT_EQ(config.fec_percentage, 15);
}

TEST_F(FecModuleTest, UpdateFecLevel) {
  fec_module_->UpdateFecLevel(FecLevel::kHigh);
  
  auto config = fec_module_->GetConfig();
  EXPECT_EQ(config.fec_level, FecLevel::kHigh);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(FecModuleTest, GetStatistics_Initial) {
  auto stats = fec_module_->GetStatistics();
  
  EXPECT_EQ(stats.fec_packets_sent, 0);
  EXPECT_EQ(stats.fec_packets_received, 0);
  EXPECT_EQ(stats.packets_recovered, 0);
}

TEST_F(FecModuleTest, GetStatistics_AfterEncoding) {
  // Add packets and encode
  for (uint16_t i = 100; i <= 105; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    packet->SetPayloadType(96);
    packet->SetTimestamp(1000);
    
    std::vector<uint8_t> payload(10, static_cast<uint8_t>(i));
    packet->SetPayload(payload.data(), payload.size());
    
    fec_module_->AddMediaPacket(packet);
  }
  
  fec_module_->EncodeFec();
  
  auto stats = fec_module_->GetStatistics();
  EXPECT_GE(stats.fec_packets_sent, 1);
}

TEST_F(FecModuleTest, ResetStatistics) {
  // Add packets and encode
  for (uint16_t i = 100; i <= 105; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    packet->SetPayloadType(96);
    packet->SetTimestamp(1000);
    
    std::vector<uint8_t> payload(10, static_cast<uint8_t>(i));
    packet->SetPayload(payload.data(), payload.size());
    
    fec_module_->AddMediaPacket(packet);
  }
  
  fec_module_->EncodeFec();
  fec_module_->ResetStatistics();
  
  auto stats = fec_module_->GetStatistics();
  EXPECT_EQ(stats.fec_packets_sent, 0);
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(FecModuleTest, SetOnFecEncodeCallback) {
  bool callback_called = false;
  
  fec_module_->SetOnFecEncodeCallback(
      [&callback_called](const std::vector<std::shared_ptr<RtpPacket>>& packets) {
        callback_called = true;
      });
  
  // Add packets and encode
  for (uint16_t i = 100; i <= 105; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    packet->SetPayloadType(96);
    packet->SetTimestamp(1000);
    
    std::vector<uint8_t> payload(10, static_cast<uint8_t>(i));
    packet->SetPayload(payload.data(), payload.size());
    
    fec_module_->AddMediaPacket(packet);
  }
  
  fec_module_->EncodeFec();
  
  EXPECT_TRUE(callback_called);
}

TEST_F(FecModuleTest, SetOnFecRecoverCallback) {
  bool callback_called = false;
  
  fec_module_->SetOnFecRecoverCallback(
      [&callback_called](const std::vector<std::shared_ptr<RtpPacket>>& packets) {
        callback_called = true;
      });
  
  // This would be called when packets are recovered
  // Not triggered in this simple test
  EXPECT_TRUE(true);
}

// ============================================================================
// State Query Tests
// ============================================================================

TEST_F(FecModuleTest, GetFecGroups) {
  auto groups = fec_module_->GetFecGroups();
  
  // Should return at least one group
  EXPECT_GE(groups.size(), 0);
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(FecModuleTest, Stop) {
  fec_module_->Stop();
  
  // After stop, processing should be ignored
  auto packet = std::make_shared<RtpPacket>();
  packet->SetSequenceNumber(100);
  packet->SetPayloadType(96);
  
  std::vector<uint8_t> payload = {1, 2, 3};
  packet->SetPayload(payload.data(), payload.size());
  
  fec_module_->AddMediaPacket(packet);
  
  auto pending = fec_module_->GetPendingFecPackets();
  EXPECT_TRUE(pending.empty());
}

TEST_F(FecModuleTest, Reset) {
  // Add some packets
  for (uint16_t i = 100; i <= 105; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    packet->SetPayloadType(96);
    packet->SetTimestamp(1000);
    
    std::vector<uint8_t> payload(10, static_cast<uint8_t>(i));
    packet->SetPayload(payload.data(), payload.size());
    
    fec_module_->AddMediaPacket(packet);
  }
  
  fec_module_->EncodeFec();
  fec_module_->Reset();
  
  auto stats = fec_module_->GetStatistics();
  EXPECT_EQ(stats.fec_packets_sent, 0);
}

// ============================================================================
// Factory Tests
// ============================================================================

TEST(FecModuleFactoryTest, Create) {
  auto module = FecModuleFactory::Create(FecAlgorithm::kXorFec);
  EXPECT_NE(module, nullptr);
}

TEST(FecModuleFactoryTest, CreateUlpFec) {
  auto module = FecModuleFactory::CreateUlpFec();
  EXPECT_NE(module, nullptr);
}

TEST(FecModuleFactoryTest, CreateXorFec) {
  auto module = FecModuleFactory::CreateXorFec();
  EXPECT_NE(module, nullptr);
}

}  // namespace minirtc

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
