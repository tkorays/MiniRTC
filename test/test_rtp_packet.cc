/**
 * @file test_rtp_packet.cc
 * @brief Unit tests for RTP packet
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include "transport/rtp_packet.h"

using namespace minirtc;

class RtpPacketTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(RtpPacketTest, DefaultConstruction) {
  RtpPacket packet;
  EXPECT_EQ(packet.GetVersion(), 2);
  EXPECT_EQ(packet.GetPayloadType(), 0);
  EXPECT_EQ(packet.GetSequenceNumber(), 0);
  EXPECT_EQ(packet.GetTimestamp(), 0);
  EXPECT_EQ(packet.GetSsrc(), 0);
  EXPECT_FALSE(packet.GetPadding());
  EXPECT_FALSE(packet.GetExtension());
  EXPECT_EQ(packet.GetCsrcCount(), 0);
  EXPECT_EQ(packet.GetMarker(), 0);
}

TEST_F(RtpPacketTest, SetFields) {
  RtpPacket packet;

  packet.SetPayloadType(96);
  packet.SetSequenceNumber(1234);
  packet.SetTimestamp(160);
  packet.SetSsrc(0x12345678);
  packet.SetMarker(1);
  packet.SetPadding(true);
  packet.SetExtension(true);

  EXPECT_EQ(packet.GetPayloadType(), 96);
  EXPECT_EQ(packet.GetSequenceNumber(), 1234);
  EXPECT_EQ(packet.GetTimestamp(), 160);
  EXPECT_EQ(packet.GetSsrc(), 0x12345678);
  EXPECT_EQ(packet.GetMarker(), 1);
  EXPECT_TRUE(packet.GetPadding());
  EXPECT_TRUE(packet.GetExtension());
}

TEST_F(RtpPacketTest, PayloadOperations) {
  RtpPacket packet;

  const uint8_t payload_data[] = {0x00, 0x01, 0x02, 0x03};
  EXPECT_EQ(packet.SetPayload(payload_data, sizeof(payload_data)), 0);
  EXPECT_EQ(packet.GetPayloadSize(), sizeof(payload_data));
  EXPECT_EQ(memcmp(packet.GetPayload(), payload_data, sizeof(payload_data)), 0);

  packet.ClearPayload();
  EXPECT_EQ(packet.GetPayloadSize(), 0);
}

TEST_F(RtpPacketTest, SerializeAndDeserialize) {
  RtpPacket original;
  original.SetPayloadType(96);
  original.SetSequenceNumber(1234);
  original.SetTimestamp(160);
  original.SetSsrc(0x12345678);
  original.SetMarker(1);

  const uint8_t payload_data[] = {0x00, 0x01, 0x02, 0x03};
  original.SetPayload(payload_data, sizeof(payload_data));

  // Serialize
  EXPECT_EQ(original.Serialize(), 0);
  EXPECT_GT(original.GetSize(), 0);

  // Deserialize
  RtpPacket parsed;
  EXPECT_EQ(parsed.Deserialize(original.GetData(), original.GetSize()), 0);

  // Verify
  EXPECT_EQ(parsed.GetVersion(), 2);
  EXPECT_EQ(parsed.GetPayloadType(), 96);
  EXPECT_EQ(parsed.GetSequenceNumber(), 1234);
  EXPECT_EQ(parsed.GetTimestamp(), 160);
  EXPECT_EQ(parsed.GetSsrc(), 0x12345678);
  EXPECT_EQ(parsed.GetMarker(), 1);
  EXPECT_EQ(parsed.GetPayloadSize(), sizeof(payload_data));
}

TEST_F(RtpPacketTest, Clone) {
  RtpPacket original;
  original.SetPayloadType(96);
  original.SetSequenceNumber(1234);
  original.SetTimestamp(160);
  original.SetSsrc(0x12345678);

  const uint8_t payload_data[] = {0x00, 0x01, 0x02, 0x03};
  original.SetPayload(payload_data, sizeof(payload_data));
  original.Serialize();

  auto cloned = original.Clone();

  EXPECT_EQ(cloned->GetPayloadType(), 96);
  EXPECT_EQ(cloned->GetSequenceNumber(), 1234);
  EXPECT_EQ(cloned->GetTimestamp(), 160);
  EXPECT_EQ(cloned->GetSsrc(), 0x12345678);
  EXPECT_EQ(cloned->GetPayloadSize(), sizeof(payload_data));
}

TEST_F(RtpPacketTest, CSRCList) {
  RtpPacket packet;

  packet.AddCsrc(0x11111111);
  packet.AddCsrc(0x22222222);
  packet.AddCsrc(0x33333333);

  EXPECT_EQ(packet.GetCsrcCount(), 3);
  EXPECT_EQ(packet.GetCsrcList().size(), 3);
  EXPECT_EQ(packet.GetCsrcList()[0], 0x11111111);
  EXPECT_EQ(packet.GetCsrcList()[1], 0x22222222);
  EXPECT_EQ(packet.GetCsrcList()[2], 0x33333333);

  packet.ClearCsrc();
  EXPECT_EQ(packet.GetCsrcCount(), 0);
  EXPECT_TRUE(packet.GetCsrcList().empty());
}

TEST_F(RtpPacketTest, Extension) {
  RtpPacket packet;
  packet.SetExtension(true);
  packet.SetExtensionProfile(0xBEDE);

  const uint8_t ext_data[] = {0x00, 0x01, 0x02, 0x03};
  packet.SetExtensionData(ext_data, sizeof(ext_data));

  EXPECT_TRUE(packet.HasExtension());
  EXPECT_EQ(packet.GetExtensionProfile(), 0xBEDE);
  EXPECT_EQ(packet.GetExtensionSize(), sizeof(ext_data));

  packet.ClearExtension();
  EXPECT_FALSE(packet.HasExtension());
}

TEST_F(RtpPacketTest, ToString) {
  RtpPacket packet;
  packet.SetPayloadType(96);
  packet.SetSequenceNumber(1234);
  packet.SetTimestamp(160);
  packet.SetSsrc(0x12345678);

  std::string str = packet.ToString();
  EXPECT_NE(str.find("PT=96"), std::string::npos);
  EXPECT_NE(str.find("Seq=1234"), std::string::npos);
}

TEST_F(RtpPacketTest, Reset) {
  RtpPacket packet;

  packet.SetPayloadType(96);
  packet.SetSequenceNumber(1234);
  packet.SetTimestamp(160);
  packet.SetSsrc(0x12345678);
  packet.SetMarker(1);

  const uint8_t payload_data[] = {0x00, 0x01, 0x02, 0x03};
  packet.SetPayload(payload_data, sizeof(payload_data));

  packet.Reset();

  EXPECT_EQ(packet.GetPayloadType(), 0);
  EXPECT_EQ(packet.GetSequenceNumber(), 0);
  EXPECT_EQ(packet.GetTimestamp(), 0);
  EXPECT_EQ(packet.GetSsrc(), 0);
  EXPECT_EQ(packet.GetMarker(), 0);
  EXPECT_EQ(packet.GetPayloadSize(), 0);
}

// Test factory function
TEST_F(RtpPacketTest, Factory) {
  auto packet = CreateRtpPacket(96, 160, 1234);
  EXPECT_NE(packet, nullptr);
  EXPECT_EQ(packet->GetPayloadType(), 96);
  EXPECT_EQ(packet->GetTimestamp(), 160);
  EXPECT_EQ(packet->GetSequenceNumber(), 1234);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
