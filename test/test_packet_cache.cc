/**
 * @file test_packet_cache.cc
 * @brief Unit tests for PacketCache class
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "minirtc/packet_cache.h"
#include "minirtc/transport/rtp_packet.h"

namespace minirtc {

// ============================================================================
// Test Fixtures
// ============================================================================

class PacketCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.max_cache_size = 10;
    config_.max_age_ms = 5000;
    cache_ = std::make_unique<PacketCache>(config_);
  }

  PacketCacheConfig config_;
  std::unique_ptr<PacketCache> cache_;
};

// ============================================================================
// Basic Operation Tests
// ============================================================================

TEST_F(PacketCacheTest, InsertPacket_Success) {
  auto packet = std::make_shared<RtpPacket>();
  packet->SetSequenceNumber(100);
  packet->SetPayloadType(96);
  packet->SetTimestamp(1000);
  
  EXPECT_TRUE(cache_->InsertPacket(packet));
  EXPECT_EQ(cache_->size(), 1);
}

TEST_F(PacketCacheTest, InsertPacket_Duplicate) {
  auto packet1 = std::make_shared<RtpPacket>();
  packet1->SetSequenceNumber(100);
  
  auto packet2 = std::make_shared<RtpPacket>();
  packet2->SetSequenceNumber(100);
  
  EXPECT_TRUE(cache_->InsertPacket(packet1));
  EXPECT_EQ(cache_->size(), 1);
  
  EXPECT_TRUE(cache_->InsertPacket(packet2));
  EXPECT_EQ(cache_->size(), 1);  // Still 1, not 2
}

TEST_F(PacketCacheTest, GetPacket_Found) {
  auto packet = std::make_shared<RtpPacket>();
  packet->SetSequenceNumber(100);
  
  cache_->InsertPacket(packet);
  
  auto retrieved = cache_->GetPacket(100);
  EXPECT_NE(retrieved, nullptr);
  EXPECT_EQ(retrieved->GetSequenceNumber(), 100);
}

TEST_F(PacketCacheTest, GetPacket_NotFound) {
  auto retrieved = cache_->GetPacket(100);
  EXPECT_EQ(retrieved, nullptr);
}

TEST_F(PacketCacheTest, HasPacket) {
  auto packet = std::make_shared<RtpPacket>();
  packet->SetSequenceNumber(100);
  
  EXPECT_FALSE(cache_->HasPacket(100));
  
  cache_->InsertPacket(packet);
  
  EXPECT_TRUE(cache_->HasPacket(100));
}

TEST_F(PacketCacheTest, RemovePacket) {
  auto packet = std::make_shared<RtpPacket>();
  packet->SetSequenceNumber(100);
  
  cache_->InsertPacket(packet);
  EXPECT_EQ(cache_->size(), 1);
  
  EXPECT_TRUE(cache_->RemovePacket(100));
  EXPECT_EQ(cache_->size(), 0);
  
  EXPECT_FALSE(cache_->RemovePacket(100));  // Already removed
}

TEST_F(PacketCacheTest, Clear) {
  for (uint16_t i = 100; i < 105; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    cache_->InsertPacket(packet);
  }
  
  EXPECT_EQ(cache_->size(), 5);
  
  cache_->Clear();
  
  EXPECT_EQ(cache_->size(), 0);
}

// ============================================================================
// Loss Detection Tests
// ============================================================================

TEST_F(PacketCacheTest, OnPacketArrived_NoLoss) {
  // Insert packets sequentially
  for (uint16_t i = 100; i < 105; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    cache_->InsertPacket(packet);
    
    auto lost = cache_->OnPacketArrived(i, 1000);
    EXPECT_TRUE(lost.empty());
  }
}

TEST_F(PacketCacheTest, OnPacketArrived_WithLoss) {
  // Insert packets with gap
  for (uint16_t i = 100; i <= 102; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    cache_->InsertPacket(packet);
    cache_->OnPacketArrived(i, 1000);
  }
  
  // Skip 103, insert 104
  auto packet104 = std::make_shared<RtpPacket>();
  packet104->SetSequenceNumber(104);
  cache_->InsertPacket(packet104);
  
  auto lost = cache_->OnPacketArrived(104, 2000);
  
  // Should detect 103 as lost
  EXPECT_EQ(lost.size(), 1);
  EXPECT_EQ(lost[0], 103);
}

TEST_F(PacketCacheTest, OnPacketArrived_MultipleLoss) {
  // Insert 100, skip 101-103, insert 104
  auto packet100 = std::make_shared<RtpPacket>();
  packet100->SetSequenceNumber(100);
  cache_->InsertPacket(packet100);
  cache_->OnPacketArrived(100, 1000);
  
  auto packet104 = std::make_shared<RtpPacket>();
  packet104->SetSequenceNumber(104);
  cache_->InsertPacket(packet104);
  
  auto lost = cache_->OnPacketArrived(104, 2000);
  
  // Should detect 101, 102, 103 as lost
  EXPECT_EQ(lost.size(), 3);
  EXPECT_EQ(lost[0], 101);
  EXPECT_EQ(lost[1], 102);
  EXPECT_EQ(lost[2], 103);
}

TEST_F(PacketCacheTest, OnPacketArrived_SequenceWrapAround) {
  // Test sequence number wrap-around (65535 -> 0)
  auto packet1 = std::make_shared<RtpPacket>();
  packet1->SetSequenceNumber(65534);
  cache_->InsertPacket(packet1);
  cache_->OnPacketArrived(65534, 1000);
  
  auto packet2 = std::make_shared<RtpPacket>();
  packet2->SetSequenceNumber(0);
  cache_->InsertPacket(packet2);
  
  auto lost = cache_->OnPacketArrived(0, 2000);
  
  // Should detect 65535 as lost
  EXPECT_EQ(lost.size(), 1);
  EXPECT_EQ(lost[0], 65535);
}

// ============================================================================
// Batch Operation Tests
// ============================================================================

TEST_F(PacketCacheTest, GetPacketsInRange) {
  for (uint16_t i = 100; i <= 110; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    cache_->InsertPacket(packet);
  }
  
  auto packets = cache_->GetPacketsInRange(102, 105);
  
  EXPECT_EQ(packets.size(), 4);
  EXPECT_EQ(packets[0]->GetSequenceNumber(), 102);
  EXPECT_EQ(packets[1]->GetSequenceNumber(), 103);
  EXPECT_EQ(packets[2]->GetSequenceNumber(), 104);
  EXPECT_EQ(packets[3]->GetSequenceNumber(), 105);
}

TEST_F(PacketCacheTest, GetAllPackets) {
  for (uint16_t i = 100; i <= 105; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    cache_->InsertPacket(packet);
  }
  
  auto packets = cache_->GetAllPackets();
  
  EXPECT_EQ(packets.size(), 6);
}

// ============================================================================
// Expiration Tests
// ============================================================================

TEST_F(PacketCacheTest, CleanupExpiredPackets) {
  // Insert packets with different timestamps
  for (uint16_t i = 100; i <= 105; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    cache_->InsertPacket(packet);
  }
  
  // Try to cleanup with current time = 1000 (all packets just inserted)
  size_t cleaned = cache_->CleanupExpiredPackets(1000);
  EXPECT_EQ(cleaned, 0);
  
  // Cleanup with time = 10000 (all expired)
  cleaned = cache_->CleanupExpiredPackets(10000);
  EXPECT_EQ(cleaned, 6);
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(PacketCacheTest, OnPacketLostCallback) {
  bool callback_called = false;
  uint16_t lost_seq = 0;
  
  cache_->SetOnPacketLostCallback([&callback_called, &lost_seq](uint16_t seq) {
    callback_called = true;
    lost_seq = seq;
  });
  
  // Insert 100, then 102, should detect 101 as lost
  auto packet100 = std::make_shared<RtpPacket>();
  packet100->SetSequenceNumber(100);
  cache_->InsertPacket(packet100);
  cache_->OnPacketArrived(100, 1000);
  
  auto packet102 = std::make_shared<RtpPacket>();
  packet102->SetSequenceNumber(102);
  cache_->InsertPacket(packet102);
  
  auto lost = cache_->OnPacketArrived(102, 2000);
  
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(lost_seq, 101);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(PacketCacheTest, GetStatistics) {
  auto stats_before = cache_->GetStatistics();
  EXPECT_EQ(stats_before.total_packets_received, 0);
  EXPECT_EQ(stats_before.total_packets_lost, 0);
  
  // Insert some packets
  for (uint16_t i = 100; i <= 105; ++i) {
    auto packet = std::make_shared<RtpPacket>();
    packet->SetSequenceNumber(i);
    cache_->InsertPacket(packet);
    cache_->OnPacketArrived(i, i * 1000);
  }
  
  auto stats_after = cache_->GetStatistics();
  EXPECT_EQ(stats_after.total_packets_received, 6);
  EXPECT_EQ(stats_after.current_seq, 105);
}

}  // namespace minirtc

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
