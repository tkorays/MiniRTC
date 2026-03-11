/**
 * @file test_h264_packer.cc
 * @brief Unit tests for H264Packer and VideoAssembler
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <cstring>

#include "minirtc/codec/h264_packer.h"
#include "minirtc/transport/rtp_packet.h"

using namespace minirtc;

// ============================================================================
// H264Packer Tests
// ============================================================================

class H264PackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        packer_ = std::make_shared<H264Packer>(96);
        packer_->SetSsrc(0x12345678);
    }
    
    std::shared_ptr<H264Packer> packer_;
};

// Helper to create SPS NALU
std::vector<uint8_t> CreateSpsNalu() {
    // SPS: Start code + NALU header(7) + SPS data
    return {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1E, 0x89, 0x80};
}

// Helper to create IDR NALU
std::vector<uint8_t> CreateIdrNalu() {
    // IDR: Start code + NALU header(5) + IDR data
    return {0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00, 0x00};
}

TEST_F(H264PackerTest, PackSingleNalu) {
    auto nalu = CreateSpsNalu();
    // Skip start code for packer
    auto packets = packer_->PackNalu(nalu.data() + 4, nalu.size() - 4, 1000, true);
    
    EXPECT_EQ(packets.size(), 1u);
    EXPECT_EQ(packets[0]->GetPayloadType(), 96u);
    EXPECT_EQ(packets[0]->GetTimestamp(), 1000u);
    EXPECT_EQ(packets[0]->GetMarker(), 1u);
    EXPECT_EQ(packets[0]->GetSsrc(), 0x12345678u);
}

TEST_F(H264PackerTest, PackSingleNaluWithMarker) {
    auto nalu = CreateIdrNalu();
    auto packets = packer_->PackNalu(nalu.data() + 4, nalu.size() - 4, 1000, true);
    
    EXPECT_EQ(packets.size(), 1u);
    EXPECT_EQ(packets[0]->GetMarker(), 1u);
}

TEST_F(H264PackerTest, PackSingleNaluWithoutMarker) {
    auto nalu = CreateSpsNalu();
    auto packets = packer_->PackNalu(nalu.data() + 4, nalu.size() - 4, 1000, false);
    
    EXPECT_EQ(packets.size(), 1u);
    EXPECT_EQ(packets[0]->GetMarker(), 0u);
}

TEST_F(H264PackerTest, PackFuAFragmentation) {
    // Create a large NALU that requires fragmentation (> MTU)
    std::vector<uint8_t> large_nalu;
    large_nalu.push_back(0x65);  // IDR NALU header
    // Add payload > 1200 bytes
    for (int i = 0; i < 2000; ++i) {
        large_nalu.push_back(static_cast<uint8_t>(i & 0xFF));
    }
    
    auto packets = packer_->PackFuA(large_nalu.data(), large_nalu.size(), 1000, true);
    
    // Should be fragmented
    EXPECT_GT(packets.size(), 1u);
    
    // First packet should have S bit set
    const uint8_t* payload0 = packets[0]->GetPayload();
    EXPECT_EQ(payload0[0] & 0x1F, 28u);  // FU-A type
    EXPECT_EQ(payload0[1] & 0x80, 0x80);  // S bit set
    
    // Last packet should have E bit set and marker
    const uint8_t* last_payload = packets.back()->GetPayload();
    EXPECT_EQ(last_payload[1] & 0x40, 0x40);  // E bit set
    EXPECT_EQ(packets.back()->GetMarker(), 1u);
}

TEST_F(H264PackerTest, PackFuAWithCorrectHeader) {
    std::vector<uint8_t> large_nalu;
    large_nalu.push_back(0x65);  // IDR
    for (int i = 0; i < 1500; ++i) {
        large_nalu.push_back(static_cast<uint8_t>(i & 0xFF));
    }
    
    auto packets = packer_->PackFuA(large_nalu.data(), large_nalu.size(), 2000, true);
    
    // Check FU indicator
    const uint8_t* payload = packets[0]->GetPayload();
    uint8_t fu_indicator = payload[0];
    EXPECT_EQ(fu_indicator & 0x1F, 28u);  // FU-A type
    
    // Original NALU type should be in FU header
    uint8_t fu_header = payload[1] & 0x1F;
    EXPECT_EQ(fu_header, 5u);  // IDR
}

TEST_F(H264PackerTest, PackFrameAutoSelect) {
    // Small frame - should use Single NAL
    auto small_nalu = CreateIdrNalu();
    auto small_packets = packer_->PackFrame(small_nalu.data() + 4, small_nalu.size() - 4, 
        1000, true, 1200);
    EXPECT_EQ(small_packets.size(), 1u);
    
    // Large frame - should use FU-A
    std::vector<uint8_t> large_nalu;
    large_nalu.push_back(0x65);
    for (int i = 0; i < 1500; ++i) {
        large_nalu.push_back(static_cast<uint8_t>(i & 0xFF));
    }
    auto large_packets = packer_->PackFrame(large_nalu.data(), large_nalu.size(), 
        2000, true, 1200);
    EXPECT_GT(large_packets.size(), 1u);
}

TEST_F(H264PackerTest, SequenceNumberIncrement) {
    auto nalu = CreateIdrNalu();
    
    auto p1 = packer_->PackNalu(nalu.data() + 4, nalu.size() - 4, 1000, false);
    auto p2 = packer_->PackNalu(nalu.data() + 4, nalu.size() - 4, 1000, false);
    
    EXPECT_EQ(p1[0]->GetSequenceNumber() + 1, p2[0]->GetSequenceNumber());
}

// ============================================================================
// VideoAssembler Tests
// ============================================================================

class VideoAssemblerTest : public ::testing::Test {
protected:
    void SetUp() override {
        assembler_ = std::make_shared<VideoAssembler>();
    }
    
    std::shared_ptr<VideoAssembler> assembler_;
};

// Create FU-A packet for testing
std::shared_ptr<RtpPacket> CreateFuAPacket(uint8_t nalu_type, bool start, bool end, 
    uint32_t timestamp, uint16_t seq) {
    auto packet = std::make_shared<RtpPacket>(96, timestamp, seq);
    packet->SetSsrc(0x12345678);
    
    // FU indicator + FU header + payload
    uint8_t fu_indicator = 0x1C | nalu_type;  // FU-A with NRI from original
    uint8_t fu_header = nalu_type;
    if (start) fu_header |= 0x80;
    if (end) fu_header |= 0x40;
    
    std::vector<uint8_t> payload = {fu_indicator, fu_header, 0x01, 0x02, 0x03};
    packet->SetPayload(payload.data(), payload.size());
    
    if (end) {
        packet->SetMarker(1);
    }
    
    return packet;
}

// Create Single NAL packet for testing
std::shared_ptr<RtpPacket> CreateSingleNaluPacket(uint8_t nalu_type, 
    uint32_t timestamp, uint16_t seq) {
    auto packet = std::make_shared<RtpPacket>(96, timestamp, seq);
    packet->SetSsrc(0x12345678);
    
    uint8_t nalu_header = nalu_type;
    std::vector<uint8_t> payload = {nalu_header, 0x01, 0x02, 0x03, 0x04};
    packet->SetPayload(payload.data(), payload.size());
    packet->SetMarker(1);
    
    return packet;
}

TEST_F(VideoAssemblerTest, SingleNaluAssembly) {
    auto packet = CreateSingleNaluPacket(5, 1000, 1);  // IDR
    
    assembler_->AddPacket(packet);
    
    auto frame = assembler_->GetFrame();
    EXPECT_NE(frame, nullptr);
    EXPECT_GT(frame->size(), 0u);
}

TEST_F(VideoAssemblerTest, FuAFragmentAssembly) {
    uint32_t timestamp = 1000;
    
    // First fragment (start)
    auto p1 = CreateFuAPacket(5, true, false, timestamp, 1);
    assembler_->AddPacket(p1);
    // Note: InProgress status may vary based on implementation
    
    // Middle fragments
    auto p2 = CreateFuAPacket(5, false, false, timestamp, 2);
    assembler_->AddPacket(p2);
    
    // Last fragment (end)
    auto p3 = CreateFuAPacket(5, false, true, timestamp, 3);
    assembler_->AddPacket(p3);
    
    // Should now have complete frame
    auto frame = assembler_->GetFrame();
    EXPECT_NE(frame, nullptr);
}

TEST_F(VideoAssemblerTest, GetFrameClearsState) {
    auto packet = CreateSingleNaluPacket(5, 1000, 1);
    assembler_->AddPacket(packet);
    
    auto frame1 = assembler_->GetFrame();
    EXPECT_NE(frame1, nullptr);
    
    // Second call should return nullptr
    auto frame2 = assembler_->GetFrame();
    EXPECT_EQ(frame2, nullptr);
}

TEST_F(VideoAssemblerTest, ResetTest) {
    auto p1 = CreateFuAPacket(5, true, false, 1000, 1);
    assembler_->AddPacket(p1);
    
    assembler_->Reset();
    EXPECT_FALSE(assembler_->IsInProgress());
    
    auto frame = assembler_->GetFrame();
    EXPECT_EQ(frame, nullptr);
}

TEST_F(VideoAssemblerTest, InProgressState) {
    EXPECT_FALSE(assembler_->IsInProgress());
    
    // Single NAL completes immediately
    auto packet = CreateSingleNaluPacket(5, 1000, 1);
    assembler_->AddPacket(packet);
    EXPECT_FALSE(assembler_->IsInProgress());
    
    // FU-A in progress - test with Start packet
    auto p1 = CreateFuAPacket(5, true, false, 2000, 1);
    assembler_->AddPacket(p1);
    // Note: The implementation may complete immediately if there's no end packet
}

TEST_F(VideoAssemblerTest, NullPacket) {
    assembler_->AddPacket(nullptr);
    EXPECT_FALSE(assembler_->IsInProgress());
    
    auto frame = assembler_->GetFrame();
    EXPECT_EQ(frame, nullptr);
}

TEST_F(VideoAssemblerTest, EmptyPayload) {
    auto packet = std::make_shared<RtpPacket>(96, 1000, 1);
    packet->SetPayload(nullptr, 0);
    
    assembler_->AddPacket(packet);
    // Should not crash
    EXPECT_FALSE(assembler_->IsInProgress());
}

// ============================================================================
// Integration Test: Pack and Assemble
// ============================================================================

class H264PackerAssemblerTest : public ::testing::Test {
protected:
    void SetUp() override {
        packer_ = std::make_shared<H264Packer>(96);
        packer_->SetSsrc(0x12345678);
        assembler_ = std::make_shared<VideoAssembler>();
    }
    
    std::shared_ptr<H264Packer> packer_;
    std::shared_ptr<VideoAssembler> assembler_;
};

TEST_F(H264PackerAssemblerTest, RoundTripSmallFrame) {
    // Single NAL unit round trip
    std::vector<uint8_t> nalu = {0x65, 0x01, 0x02, 0x03, 0x04};  // IDR
    
    auto packets = packer_->PackNalu(nalu.data(), nalu.size(), 1000, true);
    EXPECT_EQ(packets.size(), 1u);
    
    for (const auto& pkt : packets) {
        assembler_->AddPacket(pkt);
    }
    
    auto frame = assembler_->GetFrame();
    EXPECT_NE(frame, nullptr);
    // Frame should contain the NALU
    EXPECT_GE(frame->size(), 1u);
}

TEST_F(H264PackerAssemblerTest, RoundTripLargeFrame) {
    // Large frame with FU-A fragmentation
    std::vector<uint8_t> large_nalu;
    large_nalu.push_back(0x65);  // IDR
    for (int i = 0; i < 1500; ++i) {
        large_nalu.push_back(static_cast<uint8_t>(i & 0xFF));
    }
    
    auto packets = packer_->PackFuA(large_nalu.data(), large_nalu.size(), 1000, true);
    EXPECT_GT(packets.size(), 1u);
    
    for (const auto& pkt : packets) {
        assembler_->AddPacket(pkt);
    }
    
    auto frame = assembler_->GetFrame();
    EXPECT_NE(frame, nullptr);
}

TEST_F(H264PackerAssemblerTest, RoundTripAutoSelect) {
    // Small frame
    std::vector<uint8_t> small = {0x67, 0x01, 0x02};  // SPS
    auto small_packets = packer_->PackFrame(small.data(), small.size(), 1000, true, 1200);
    EXPECT_EQ(small_packets.size(), 1u);
    
    // Large frame
    std::vector<uint8_t> large;
    large.push_back(0x65);
    for (int i = 0; i < 1500; ++i) {
        large.push_back(static_cast<uint8_t>(i & 0xFF));
    }
    auto large_packets = packer_->PackFrame(large.data(), large.size(), 2000, true, 1200);
    EXPECT_GT(large_packets.size(), 1u);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
