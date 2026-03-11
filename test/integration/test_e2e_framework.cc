/**
 * @file test_e2e_framework.cc
 * @brief End-to-end test framework tests
 */

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>

#include "minirtc/e2e_test.h"
#include "minirtc/transport/rtp_transport.h"
#include "minirtc/transport/transport_types.h"

using namespace minirtc;

// ============================================================================
// E2E Framework Tests
// ============================================================================

class E2ETestFrameworkTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.local_port_a = 9000;
        config_.local_port_b = 9002;
        config_.remote_ip = "127.0.0.1";
        config_.remote_port_a = 9002;
        config_.remote_port_b = 9000;
        config_.enable_rtcp = false;
    }
    
    void TearDown() override {
        // Clean up ports
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    E2EConfig config_;
};

TEST_F(E2ETestFrameworkTest, CreateE2ETest) {
    E2ETest test;
    EXPECT_TRUE(test.Initialize(config_));
}

// ============================================================================
// Socket Communication Tests
// ============================================================================

class SocketCommunicationTest : public ::testing::Test {
protected:
    void SetUp() override {
        transport_a_ = CreateRTPTransport();
        transport_b_ = CreateRTPTransport();
        
        config_a_.local_addr.ip = "127.0.0.1";
        config_a_.local_addr.port = 9100;
        config_a_.remote_addr.ip = "127.0.0.1";
        config_a_.remote_addr.port = 9102;
        config_a_.enable_rtcp = false;
        
        config_b_.local_addr.ip = "127.0.0.1";
        config_b_.local_addr.port = 9102;
        config_b_.remote_addr.ip = "127.0.0.1";
        config_b_.remote_addr.port = 9100;
        config_b_.enable_rtcp = false;
    }
    
    void TearDown() override {
        if (transport_a_) transport_a_->Close();
        if (transport_b_) transport_b_->Close();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::shared_ptr<IRTPTransport> transport_a_;
    std::shared_ptr<IRTPTransport> transport_b_;
    RtpTransportConfig config_a_;
    RtpTransportConfig config_b_;
};

TEST_F(SocketCommunicationTest, OpenAndClose) {
    EXPECT_EQ(transport_a_->Open(config_a_), TransportError::kOk);
    EXPECT_EQ(transport_a_->GetState(), TransportState::kOpen);
    
    transport_a_->Close();
    EXPECT_EQ(transport_a_->GetState(), TransportState::kClosed);
}

TEST_F(SocketCommunicationTest, BidirectionalOpen) {
    EXPECT_EQ(transport_a_->Open(config_a_), TransportError::kOk);
    EXPECT_EQ(transport_b_->Open(config_b_), TransportError::kOk);
    
    EXPECT_EQ(transport_a_->GetState(), TransportState::kOpen);
    EXPECT_EQ(transport_b_->GetState(), TransportState::kOpen);
}

// ============================================================================
// Transport Stats Tests
// ============================================================================

class TransportStatsTest : public ::testing::Test {
protected:
    void SetUp() override {
        transport_ = CreateRTPTransport();
        config_.local_addr.ip = "127.0.0.1";
        config_.local_addr.port = 9200;
        config_.remote_addr.ip = "127.0.0.1";
        config_.remote_addr.port = 9202;
        config_.enable_rtcp = false;
    }
    
    void TearDown() override {
        transport_->Close();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::shared_ptr<IRTPTransport> transport_;
    RtpTransportConfig config_;
};

TEST_F(TransportStatsTest, InitialStats) {
    transport_->Open(config_);
    
    auto stats = transport_->GetStats();
    EXPECT_EQ(stats.packets_sent, 0u);
    EXPECT_EQ(stats.packets_received, 0u);
    EXPECT_EQ(stats.bytes_sent, 0u);
    EXPECT_EQ(stats.bytes_received, 0u);
}

TEST_F(TransportStatsTest, StatsAfterSend) {
    transport_->Open(config_);
    
    // Create and send packet
    auto packet = std::make_shared<RtpPacket>(96, 1000, 1);
    packet->SetSsrc(0x12345678);
    uint8_t payload[] = {0x01, 0x02};
    packet->SetPayload(payload, sizeof(payload));
    packet->Serialize();
    
    transport_->SendRtpPacket(packet);
    
    auto stats = transport_->GetStats();
    EXPECT_GT(stats.packets_sent, 0u);
    EXPECT_GT(stats.bytes_sent, 0u);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
