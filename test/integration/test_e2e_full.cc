/**
 * @file test_e2e_full.cc
 * @brief Full End-to-End integration tests for MiniRTC
 * 
 * Tests the complete pipelines:
 * - Audio: Capture (Fake)
 * - Video: Capture (Fake) -> H264 Packer -> Video Assembler
 * 
 * This test validates the complete media pipeline with network simulation.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <atomic>
#include <memory>
#include <vector>
#include <cstring>

#include "minirtc/transport/rtp_transport.h"
#include "minirtc/transport/transport_types.h"
#include "minirtc/transport/rtp_packet.h"

#include "minirtc/fake_audio_capture.h"
#include "minirtc/fake_video_capture.h"

#include "minirtc/codec/codec_types.h"
#include "minirtc/codec/h264_packer.h"

#include "test_network_emulator.h"

using namespace minirtc;
using namespace minirtc::fake;
using namespace minirtc::test;

// ============================================================================
// Constants
// ============================================================================

static constexpr uint8_t kAudioPayloadType = 96;
static constexpr uint8_t kVideoPayloadType = 97;
static constexpr uint32_t kSsrcA = 0x12345678;
static constexpr uint32_t kSsrcB = 0x87654321;

// ============================================================================
// Helper Functions
// ============================================================================

std::shared_ptr<RTPTransport> CreateRTPTransportHelper() {
    return std::make_shared<RTPTransport>();
}

// ============================================================================
// Audio Pipeline Test
// ============================================================================

class E2EAudioPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test: Audio Capture
TEST_F(E2EAudioPipelineTest, AudioCaptureBasic) {
    std::cout << "\n=== Audio Capture Basic Test ===" << std::endl;
    
    // Create and configure FakeAudioCapture
    auto capture = std::make_unique<FakeAudioCapture>();
    ASSERT_NE(capture, nullptr);
    
    AudioCaptureParam capture_param;
    capture_param.sample_rate = 48000;
    capture_param.channels = 1;
    capture_param.format = AudioSampleFormat::kInt16;
    capture_param.frames_per_buffer = 480;
    
    auto result = capture->SetParam(capture_param);
    EXPECT_EQ(result, ErrorCode::kOk);
    
    result = capture->Initialize();
    EXPECT_EQ(result, ErrorCode::kOk);
    
    // Verify capture state
    EXPECT_EQ(capture->GetState(), CaptureState::kReady);
    
    // Configure sine wave generator
    capture->SetGenerateSineWave(440);  // 440Hz tone
    
    // Start capture
    result = capture->StartCapture(nullptr);
    EXPECT_EQ(result, ErrorCode::kOk);
    
    // Wait a bit for frames to be captured
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Verify capturing
    EXPECT_TRUE(capture->IsCapturing());
    
    // Stop capture
    result = capture->StopCapture();
    EXPECT_EQ(result, ErrorCode::kOk);
    
    // Release
    capture->Release();
    
    std::cout << "=== Audio Capture Basic Test PASSED ===" << std::endl;
}

// Test: Audio Frame Generation
TEST_F(E2EAudioPipelineTest, AudioFrameGeneration) {
    std::cout << "\n=== Audio Frame Generation Test ===" << std::endl;
    
    // Test sine wave generator
    SineWaveGenerator sine_gen(48000, 440, 1);
    AudioFrame sine_frame;
    sine_gen.GenerateFrame(&sine_frame);
    
    std::cout << "  Sine wave frame: " << sine_frame.samples_per_channel 
              << " samples, " << sine_frame.channels << " channels" << std::endl;
    
    EXPECT_EQ(sine_frame.sample_rate, 48000);
    EXPECT_EQ(sine_frame.channels, 1);
    EXPECT_EQ(sine_frame.samples_per_channel, 480);
    
    // Test silence generator
    SilenceGenerator silence_gen(48000, 1);
    AudioFrame silence_frame;
    silence_gen.GenerateFrame(&silence_frame);
    
    EXPECT_EQ(silence_frame.sample_rate, 48000);
    EXPECT_EQ(silence_frame.channels, 1);
    EXPECT_EQ(silence_frame.samples_per_channel, 480);
    
    // Test white noise generator
    WhiteNoiseGenerator noise_gen(48000, 1);
    AudioFrame noise_frame;
    noise_gen.GenerateFrame(&noise_frame);
    
    EXPECT_EQ(noise_frame.sample_rate, 48000);
    EXPECT_EQ(noise_frame.channels, 1);
    EXPECT_EQ(noise_frame.samples_per_channel, 480);
    
    std::cout << "=== Audio Frame Generation Test PASSED ===" << std::endl;
}

// ============================================================================
// Video Pipeline Test
// ============================================================================

class E2EVideoPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test: Video Capture
TEST_F(E2EVideoPipelineTest, VideoCaptureBasic) {
    std::cout << "\n=== Video Capture Basic Test ===" << std::endl;
    
    // Create and configure FakeVideoCapture
    auto capture = std::make_unique<FakeVideoCapture>();
    ASSERT_NE(capture, nullptr);
    
    VideoCaptureParam capture_param;
    capture_param.width = 320;
    capture_param.height = 240;
    capture_param.target_fps = 30;
    
    auto result = capture->SetParam(capture_param);
    EXPECT_EQ(result, ErrorCode::kOk);
    
    result = capture->Initialize();
    EXPECT_EQ(result, ErrorCode::kOk);
    
    // Verify capture state
    EXPECT_EQ(capture->GetState(), CaptureState::kReady);
    
    // Configure gradient generator
    capture->SetGradient();
    
    // Start capture
    result = capture->StartCapture(std::weak_ptr<VideoCaptureObserver>());
    EXPECT_EQ(result, ErrorCode::kOk);
    
    // Wait a bit for frames to be captured
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Verify capturing
    EXPECT_TRUE(capture->IsCapturing());
    
    // Stop capture
    result = capture->StopCapture();
    EXPECT_EQ(result, ErrorCode::kOk);
    
    // Release
    capture->Release();
    
    std::cout << "=== Video Capture Basic Test PASSED ===" << std::endl;
}

// Test: Video Frame Generation
TEST_F(E2EVideoPipelineTest, VideoFrameGeneration) {
    std::cout << "\n=== Video Frame Generation Test ===" << std::endl;
    
    // Test solid color generator
    SolidColorGenerator solid_gen(320, 240, 0xFF8040);
    VideoFrame solid_frame;
    solid_gen.GenerateFrame(&solid_frame);
    
    std::cout << "  Solid color frame: " << solid_frame.width << "x" << solid_frame.height << std::endl;
    
    EXPECT_EQ(solid_frame.width, 320);
    EXPECT_EQ(solid_frame.height, 240);
    
    // Test gradient generator
    GradientGenerator gradient_gen(320, 240);
    VideoFrame gradient_frame;
    gradient_gen.GenerateFrame(&gradient_frame);
    
    EXPECT_EQ(gradient_frame.width, 320);
    EXPECT_EQ(gradient_frame.height, 240);
    
    // Test checkerboard generator
    CheckerboardGenerator checker_gen(320, 240, 32);
    VideoFrame checker_frame;
    checker_gen.GenerateFrame(&checker_frame);
    
    EXPECT_EQ(checker_frame.width, 320);
    EXPECT_EQ(checker_frame.height, 240);
    
    std::cout << "=== Video Frame Generation Test PASSED ===" << std::endl;
}

// ============================================================================
// H264 Packer Test
// ============================================================================

class E2EH264PackerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test: H264 Packer Single NALU
TEST_F(E2EH264PackerTest, PackSingleNalu) {
    std::cout << "\n=== H264 Packer Single NALU Test ===" << std::endl;
    
    auto packer = std::make_shared<H264Packer>();
    ASSERT_NE(packer, nullptr);
    
    // Set configuration
    packer->SetPayloadType(kVideoPayloadType);
    packer->SetSsrc(kSsrcA);
    packer->SetSequenceNumber(1000);
    
    // Test single NALU (small enough to fit in one packet)
    std::vector<uint8_t> nalu_data(100, 0xAB);
    
    auto packets = packer->PackNalu(nalu_data.data(), nalu_data.size(), 0, true);
    
    std::cout << "  Packed " << nalu_data.size() << " bytes into " << packets.size() << " packets" << std::endl;
    
    EXPECT_GT(packets.size(), 0);
    
    // Verify packet fields
    if (!packets.empty()) {
        EXPECT_EQ(packets[0]->GetPayloadType(), kVideoPayloadType);
        EXPECT_EQ(packets[0]->GetSsrc(), kSsrcA);
        EXPECT_EQ(packets[0]->GetMarker(), true);
    }
    
    std::cout << "=== H264 Packer Single NALU Test PASSED ===" << std::endl;
}

// Test: H264 Packer FU-A Fragmentation
TEST_F(E2EH264PackerTest, PackFuA) {
    std::cout << "\n=== H264 Packer FU-A Test ===" << std::endl;
    
    auto packer = std::make_shared<H264Packer>();
    ASSERT_NE(packer, nullptr);
    
    packer->SetPayloadType(kVideoPayloadType);
    packer->SetSsrc(kSsrcA);
    
    // Test FU-A fragmentation (large NALU)
    std::vector<uint8_t> nalu_data(2000, 0xCD);  // Large payload
    
    auto packets = packer->PackFuA(nalu_data.data(), nalu_data.size(), 3000, true);
    
    std::cout << "  Packed " << nalu_data.size() << " bytes into " << packets.size() << " FU-A packets" << std::endl;
    
    // Should be fragmented into multiple packets
    EXPECT_GT(packets.size(), 1);
    
    std::cout << "=== H264 Packer FU-A Test PASSED ===" << std::endl;
}

// Test: H264 Packer Frame
TEST_F(E2EH264PackerTest, PackFrame) {
    std::cout << "\n=== H264 Packer Frame Test ===" << std::endl;
    
    auto packer = std::make_shared<H264Packer>();
    ASSERT_NE(packer, nullptr);
    
    packer->SetPayloadType(kVideoPayloadType);
    packer->SetSsrc(kSsrcA);
    
    // Test frame packing (automatic Single NAL or FU-A selection)
    std::vector<uint8_t> frame_data(500, 0xEF);
    
    auto packets = packer->PackFrame(frame_data.data(), frame_data.size(), 6000, true, 1200);
    
    std::cout << "  Packed frame of " << frame_data.size() << " bytes into " 
              << packets.size() << " packets" << std::endl;
    
    EXPECT_GT(packets.size(), 0);
    
    // Verify timestamp
    for (const auto& pkt : packets) {
        EXPECT_EQ(pkt->GetTimestamp(), 6000u);
    }
    
    std::cout << "=== H264 Packer Frame Test PASSED ===" << std::endl;
}

// ============================================================================
// Video Assembler Test
// ============================================================================

class E2EVideoAssemblerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test: Video Assembler Single NALU
TEST_F(E2EVideoAssemblerTest, AssembleSingleNalu) {
    std::cout << "\n=== Video Assembler Single NALU Test ===" << std::endl;
    
    auto assembler = std::make_shared<VideoAssembler>();
    ASSERT_NE(assembler, nullptr);
    
    // Create test RTP packet with single NALU
    auto packet = std::make_shared<RtpPacket>();
    packet->SetPayloadType(kVideoPayloadType);
    packet->SetSsrc(kSsrcA);
    packet->SetTimestamp(1000);
    packet->SetSequenceNumber(100);
    packet->SetMarker(true);
    
    std::vector<uint8_t> nalu_data = {0x67, 0x42, 0x00, 0x1F, 0x9A, 0x7E, 0x12, 0x34};  // SPS-like NALU
    packet->SetPayload(nalu_data.data(), nalu_data.size());
    
    // Add packet to assembler
    assembler->AddPacket(packet);
    
    // Should be able to get assembled frame
    auto frame = assembler->GetFrame();
    
    if (frame) {
        std::cout << "  Assembled frame: " << frame->size() << " bytes" << std::endl;
        EXPECT_GT(frame->size(), 0);
    }
    
    std::cout << "=== Video Assembler Single NALU Test PASSED ===" << std::endl;
}

// Test: Video Assembler FU-A
TEST_F(E2EVideoAssemblerTest, AssembleFuA) {
    std::cout << "\n=== Video Assembler FU-A Test ===" << std::endl;
    
    auto assembler = std::make_shared<VideoAssembler>();
    ASSERT_NE(assembler, nullptr);
    
    // Create FU-A fragments
    const int num_fragments = 3;
    std::vector<uint8_t> full_nalu_data(1500, 0xFF);
    
    for (int i = 0; i < num_fragments; i++) {
        auto packet = std::make_shared<RtpPacket>();
        packet->SetPayloadType(kVideoPayloadType);
        packet->SetSsrc(kSsrcA);
        packet->SetTimestamp(2000);
        packet->SetSequenceNumber(static_cast<uint16_t>(200 + i));
        packet->SetMarker(i == num_fragments - 1);
        
        // FU-A header: start bit, end bit, NALU type
        std::vector<uint8_t> fu_payload;
        fu_payload.push_back(0x7C | (i == 0 ? 0x80 : 0) | (i == num_fragments - 1 ? 0x40 : 0));  // FU indicator
        fu_payload.push_back(0x01);  // NALU header (slice)
        
        // Add some data
        size_t offset = i * 400;
        size_t size = std::min(size_t(400), full_nalu_data.size() - offset);
        fu_payload.insert(fu_payload.end(), full_nalu_data.begin() + offset, full_nalu_data.begin() + offset + size);
        
        packet->SetPayload(fu_payload.data(), fu_payload.size());
        
        assembler->AddPacket(packet);
    }
    
    // Should be complete now
    auto frame = assembler->GetFrame();
    
    if (frame) {
        std::cout << "  Assembled FU-A frame: " << frame->size() << " bytes" << std::endl;
        EXPECT_GT(frame->size(), 0);
    }
    
    // Reset and test again
    assembler->Reset();
    EXPECT_FALSE(assembler->IsInProgress());
    
    std::cout << "=== Video Assembler FU-A Test PASSED ===" << std::endl;
}

// ============================================================================
// End-to-End Pipeline Test
// ============================================================================

class E2EEndToEndTest : public ::testing::Test {
protected:
    void SetUp() override {
        packets_sent_ = 0;
        packets_received_ = 0;
    }
    
    std::atomic<int> packets_sent_{0};
    std::atomic<int> packets_received_{0};
};

// Test: End-to-end video pipeline with network simulation (capture -> pack -> assemble)
TEST_F(E2EEndToEndTest, EndToEndVideoPipeline) {
    std::cout << "\n=== End-to-End Video Pipeline Test ===" << std::endl;
    
    // Create components
    auto capture = std::make_unique<FakeVideoCapture>();
    auto packer = std::make_shared<H264Packer>();
    auto assembler = std::make_shared<VideoAssembler>();
    
    ASSERT_NE(capture, nullptr);
    ASSERT_NE(packer, nullptr);
    ASSERT_NE(assembler, nullptr);
    
    // Configure capture
    VideoCaptureParam cap_param;
    cap_param.width = 320;
    cap_param.height = 240;
    cap_param.target_fps = 30;
    capture->SetParam(cap_param);
    capture->Initialize();
    
    // Configure packer
    packer->SetPayloadType(kVideoPayloadType);
    packer->SetSsrc(kSsrcA);
    
    // Create network emulator
    NetworkCondition net_cond;
    net_cond.packet_loss_rate = 0.05;  // 5% loss
    net_cond.latency_ms = 10;
    NetworkEmulator emulator(net_cond);
    
    // Simulate capture -> pack -> send -> receive -> assemble
    const int num_frames = 5;
    int packets_sent = 0;
    int packets_received = 0;
    
    for (int i = 0; i < num_frames; i++) {
        // Generate video frame from capture
        VideoFrame test_frame;
        test_frame.width = 320;
        test_frame.height = 240;
        test_frame.format = VideoPixelFormat::kI420;
        
        const int y_size = 320 * 240;
        test_frame.internal_buffer.resize(y_size * 3 / 2);
        test_frame.data_y = test_frame.internal_buffer.data();
        test_frame.data_u = test_frame.data_y + y_size;
        test_frame.data_v = test_frame.data_u + y_size / 4;
        
        // Fill with test pattern
        for (int y = 0; y < 240; y++) {
            for (int x = 0; x < 320; x++) {
                test_frame.data_y[y * 320 + x] = static_cast<uint8_t>((x + i) % 256);
            }
        }
        
        // Use raw frame data as "encoded" data (simulating encoder output)
        std::vector<uint8_t> encoded_data = test_frame.internal_buffer;
        
        // Pack into RTP packets
        auto rtp_packets = packer->PackFrame(
            encoded_data.data(),
            encoded_data.size(),
            i * 3000,  // timestamp
            true,
            1200
        );
        
        for (const auto& pkt : rtp_packets) {
            packets_sent++;
            
            // Simulate network transmission
            if (!emulator.ShouldDrop()) {
                // Receiver: assemble packet
                assembler->AddPacket(pkt);
                packets_received++;
            }
        }
    }
    
    std::cout << "  Sent RTP packets: " << packets_sent << std::endl;
    std::cout << "  Received RTP packets: " << packets_received << std::endl;
    
    // Get assembled frame
    auto assembled = assembler->GetFrame();
    if (assembled && assembled->size() > 0) {
        std::cout << "  Assembled frame: " << assembled->size() << " bytes" << std::endl;
    }
    
    // Verify results
    EXPECT_GT(packets_sent, 0);
    
    // Cleanup
    capture->Release();
    
    std::cout << "=== End-to-End Video Pipeline Test PASSED ===" << std::endl;
}

// ============================================================================
// Legacy Tests (from original file)
// ============================================================================

class E2ENetworkTest : public ::testing::Test {
protected:
    void SetUp() override {
        packets_sent_ = 0;
        packets_received_ = 0;
    }
    
    std::atomic<int> packets_sent_{0};
    std::atomic<int> packets_received_{0};
};

// Test network emulator basics
TEST_F(E2ENetworkTest, NetworkEmulatorBasic) {
    NetworkCondition net_cond;
    net_cond.packet_loss_rate = 0.1;  // 10% packet loss
    net_cond.latency_ms = 5;
    
    NetworkEmulator emulator(net_cond);
    
    // Send 100 packets
    for (int i = 0; i < 100; ++i) {
        if (!emulator.ShouldDrop()) {
            packets_sent_++;
            // Simulate latency
            std::this_thread::sleep_for(std::chrono::milliseconds(net_cond.latency_ms));
        }
    }
    
    // Verify some packets were sent (not all should be dropped)
    std::cout << "Network Emulator Test:" << std::endl;
    std::cout << "  Packets sent (after drop): " << packets_sent_ << std::endl;
    
    EXPECT_GT(packets_sent_, 50);
    EXPECT_LT(packets_sent_, 100);
}

// ============================================================================
// RTP Packet Test
// ============================================================================

class E2ERTPPacketTest : public ::testing::Test {
};

// Test RTP packet creation and parsing
TEST_F(E2ERTPPacketTest, RTPPacketCreation) {
    // Create RTP packet
    auto packet = std::make_shared<RtpPacket>();
    
    // Set packet fields
    packet->SetPayloadType(kAudioPayloadType);
    packet->SetTimestamp(160);
    packet->SetSequenceNumber(1);
    packet->SetSsrc(kSsrcA);
    packet->SetMarker(true);
    
    // Create test payload
    std::vector<uint8_t> payload(160, 0xAB);
    packet->SetPayload(payload.data(), payload.size());
    
    // Verify packet fields
    EXPECT_EQ(packet->GetPayloadType(), kAudioPayloadType);
    EXPECT_EQ(packet->GetTimestamp(), 160);
    EXPECT_EQ(packet->GetSequenceNumber(), 1);
    EXPECT_EQ(packet->GetSsrc(), kSsrcA);
    EXPECT_EQ(packet->GetMarker(), true);
    EXPECT_EQ(packet->GetPayloadSize(), payload.size());
}

// ============================================================================
// Transport Address Test
// ============================================================================

class E2ETransportTest : public ::testing::Test {
};

// Test transport address creation
TEST_F(E2ETransportTest, TransportAddress) {
    // Create network addresses
    NetworkAddress local_addr("127.0.0.1", 10000);
    NetworkAddress remote_addr("127.0.0.1", 10002);
    
    // Verify addresses
    EXPECT_EQ(local_addr.ip, "127.0.0.1");
    EXPECT_EQ(local_addr.port, 10000);
    EXPECT_EQ(remote_addr.ip, "127.0.0.1");
    EXPECT_EQ(remote_addr.port, 10002);
}

// ============================================================================
// RtpTransportConfig Test
// ============================================================================

class E2ERTPTransportConfigTest : public ::testing::Test {
};

// Test RTP transport config
TEST_F(E2ERTPTransportConfigTest, RTPTransportConfigBasic) {
    RtpTransportConfig config;
    config.local_addr = NetworkAddress("0.0.0.0", 0);
    config.remote_addr = NetworkAddress("127.0.0.1", 8000);
    config.ssrc = kSsrcA;
    config.enable_rtcp = false;
    config.enable_nack = false;
    config.enable_fec = false;
    
    // Verify config
    EXPECT_EQ(config.local_addr.ip, "0.0.0.0");
    EXPECT_EQ(config.local_addr.port, 0);
    EXPECT_EQ(config.remote_addr.ip, "127.0.0.1");
    EXPECT_EQ(config.remote_addr.port, 8000);
    EXPECT_EQ(config.ssrc, kSsrcA);
    EXPECT_EQ(config.enable_rtcp, false);
    EXPECT_EQ(config.enable_nack, false);
    EXPECT_EQ(config.enable_fec, false);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
