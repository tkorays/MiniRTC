/**
 * @file test_rtcp_packet.cc
 * @brief Unit tests for RTCP packet
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include "transport/rtcp_packet.h"

using namespace minirtc;

class RtcpPacketTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(RtcpPacketTest, SrPacketSerialization) {
  auto sr = CreateRtcpSr(0x12345678);
  sr->SetNtpTimestamp(0x12345678, 0x90ABCDEF);
  sr->SetRtpTimestamp(160);
  sr->SetPacketCount(100);
  sr->SetOctetCount(10000);

  size_t size = sr->GetSize();
  EXPECT_GE(size, 28u);  // Minimum SR size

  std::vector<uint8_t> buffer(size);
  EXPECT_GE(sr->Serialize(buffer.data(), buffer.size()), 0);

  // Verify header
  EXPECT_EQ(buffer[0] & 0x1F, 0);  // RC = 0
  EXPECT_EQ(buffer[1], static_cast<uint8_t>(RtcpPacketType::kSR));
}

TEST_F(RtcpPacketTest, SrPacketDeserialization) {
  auto original = CreateRtcpSr(0x12345678);
  original->SetNtpTimestamp(0x12345678, 0x90ABCDEF);
  original->SetRtpTimestamp(160);
  original->SetPacketCount(100);
  original->SetOctetCount(10000);

  std::vector<uint8_t> buffer(original->GetSize());
  original->Serialize(buffer.data(), buffer.size());

  // Deserialize
  auto sr = std::make_shared<RtcpSrPacket>();
  EXPECT_EQ(sr->Deserialize(buffer.data(), buffer.size()), 0);

  EXPECT_EQ(sr->GetType(), RtcpPacketType::kSR);
  EXPECT_EQ(sr->GetSsrc(), 0x12345678);
  EXPECT_EQ(sr->GetRtpTimestamp(), 160);
  EXPECT_EQ(sr->GetPacketCount(), 100);
  EXPECT_EQ(sr->GetOctetCount(), 10000);
}

TEST_F(RtcpPacketTest, RrPacketSerialization) {
  auto rr = CreateRtcpRr(0x12345678);

  RtcpReportBlock block;
  block.ssrc = 0x87654321;
  block.fraction_lost = 10;
  block.packets_lost = 50;
  block.highest_seq = 1000;
  block.jitter = 20;
  block.lsr = 0;
  block.dlsr = 0;
  rr->AddReportBlock(block);

  size_t size = rr->GetSize();
  EXPECT_GE(size, 32u);  // RR + 1 report block

  std::vector<uint8_t> buffer(size);
  EXPECT_GE(rr->Serialize(buffer.data(), buffer.size()), 0);
}

TEST_F(RtcpPacketTest, RrPacketDeserialization) {
  auto original = CreateRtcpRr(0x12345678);

  RtcpReportBlock block;
  block.ssrc = 0x87654321;
  block.fraction_lost = 10;
  block.packets_lost = 50;
  block.highest_seq = 1000;
  block.jitter = 20;
  block.lsr = 0;
  block.dlsr = 0;
  original->AddReportBlock(block);

  std::vector<uint8_t> buffer(original->GetSize());
  original->Serialize(buffer.data(), buffer.size());

  // Deserialize
  auto rr = std::make_shared<RtcpRrPacket>();
  EXPECT_EQ(rr->Deserialize(buffer.data(), buffer.size()), 0);

  EXPECT_EQ(rr->GetType(), RtcpPacketType::kRR);
  EXPECT_EQ(rr->GetSsrc(), 0x12345678);
  EXPECT_EQ(rr->GetReportBlocks().size(), 1);
  EXPECT_EQ(rr->GetReportBlocks()[0].ssrc, 0x87654321);
}

TEST_F(RtcpPacketTest, SdesPacket) {
  auto sdes = CreateRtcpSdes();
  sdes->SetCname(0x12345678, "test@example.com");
  sdes->SetName(0x12345678, "Test User");

  size_t size = sdes->GetSize();
  EXPECT_GT(size, 0u);

  std::vector<uint8_t> buffer(size);
  EXPECT_GE(sdes->Serialize(buffer.data(), buffer.size()), 0);

  // Deserialize
  auto parsed = std::make_shared<RtcpSdesPacket>();
  EXPECT_EQ(parsed->Deserialize(buffer.data(), buffer.size()), 0);
}

TEST_F(RtcpPacketTest, ByePacket) {
  auto bye = CreateRtcpBye({0x12345678, 0x87654321}, "Goodbye");

  size_t size = bye->GetSize();
  EXPECT_GT(size, 0u);

  std::vector<uint8_t> buffer(size);
  EXPECT_GE(bye->Serialize(buffer.data(), buffer.size()), 0);

  // Deserialize
  auto parsed = std::make_shared<RtcpByePacket>();
  EXPECT_EQ(parsed->Deserialize(buffer.data(), buffer.size()), 0);

  EXPECT_EQ(parsed->GetSsrcs().size(), 2);
  EXPECT_EQ(parsed->GetReason(), "Goodbye");
}

TEST_F(RtcpPacketTest, NackPacket) {
  auto nack = CreateRtcpNack(0x12345678, 0x87654321, {100, 101, 102, 105, 106});

  size_t size = nack->GetSize();
  EXPECT_GT(size, 0u);

  std::vector<uint8_t> buffer(size);
  EXPECT_GE(nack->Serialize(buffer.data(), buffer.size()), 0);

  // Deserialize
  auto parsed = std::make_shared<RtcpNackPacket>();
  EXPECT_EQ(parsed->Deserialize(buffer.data(), buffer.size()), 0);

  EXPECT_EQ(parsed->GetMediaSsrc(), 0x87654321);
  EXPECT_GE(parsed->GetNackList().size(), 2u);
}

TEST_F(RtcpPacketTest, CompoundPacket) {
  auto compound = CreateRtcpCompound();

  // Add SR
  auto sr = CreateRtcpSr(0x12345678);
  sr->SetNtpTimestamp(0x12345678, 0x90ABCDEF);
  sr->SetRtpTimestamp(160);
  sr->SetPacketCount(100);
  sr->SetOctetCount(10000);
  compound->AddPacket(sr);

  // Add SDES
  auto sdes = CreateRtcpSdes();
  sdes->SetCname(0x12345678, "test@example.com");
  compound->AddPacket(sdes);

  // Serialize
  size_t size = compound->GetSize();
  std::vector<uint8_t> buffer(size);
  EXPECT_GE(compound->Serialize(buffer.data(), buffer.size()), 0);

  // Deserialize
  auto parsed = CreateRtcpCompound();
  EXPECT_EQ(parsed->Deserialize(buffer.data(), buffer.size()), 0);
  EXPECT_EQ(parsed->GetPacketCount(), 2);
}

TEST_F(RtcpPacketTest, ReportBlock) {
  RtcpSrPacket sr(0x12345678);

  RtcpReportBlock block1;
  block1.ssrc = 0x11111111;
  block1.fraction_lost = 5;
  block1.packets_lost = 10;
  block1.highest_seq = 500;
  block1.jitter = 3;
  block1.lsr = 0;
  block1.dlsr = 0;
  sr.AddReportBlock(block1);

  RtcpReportBlock block2;
  block2.ssrc = 0x22222222;
  block2.fraction_lost = 10;
  block2.packets_lost = 20;
  block2.highest_seq = 1000;
  block2.jitter = 5;
  block2.lsr = 0;
  block2.dlsr = 0;
  sr.AddReportBlock(block2);

  EXPECT_EQ(sr.GetReportBlocks().size(), 2);

  // Serialize and verify
  std::vector<uint8_t> buffer(sr.GetSize());
  sr.Serialize(buffer.data(), buffer.size());

  // Verify RC field
  EXPECT_EQ(buffer[0] & 0x1F, 2);
}

TEST_F(RtcpPacketTest, SrNtpTimestamp) {
  auto sr = CreateRtcpSr(0x12345678);
  sr->SetNtpTimestampNow();

  // NTP timestamp should be reasonable
  EXPECT_GT(sr->GetNtpTimestampHigh(), 0u);
}

TEST_F(RtcpPacketTest, GetSize) {
  // SR without report blocks
  auto sr1 = CreateRtcpSr(0x12345678);
  EXPECT_EQ(sr1->GetSize(), 28u);  // 4 + 24

  // SR with 1 report block
  auto sr2 = CreateRtcpSr(0x12345678);
  RtcpReportBlock block;
  block.ssrc = 0x87654321;
  block.fraction_lost = 0;
  block.packets_lost = 0;
  block.highest_seq = 0;
  block.jitter = 0;
  block.lsr = 0;
  block.dlsr = 0;
  sr2->AddReportBlock(block);
  EXPECT_EQ(sr2->GetSize(), 52u);  // 4 + 24 + 24

  // RR
  auto rr = CreateRtcpRr(0x12345678);
  rr->AddReportBlock(block);
  EXPECT_EQ(rr->GetSize(), 52u);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
