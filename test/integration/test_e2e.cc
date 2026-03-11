/**
 * @file test_e2e.cc
 * @brief End-to-End integration tests for MiniRTC
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <iostream>

#include "minirtc/e2e_test.h"

using namespace minirtc;

// ============================================================================
// E2E Test Fixture
// ============================================================================

class E2ETestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // Use ports in unprivileged range
        config_.local_port_a = 10000;
        config_.local_port_b = 10002;
        config_.remote_ip = "127.0.0.1";
        config_.remote_port_a = 10002;
        config_.remote_port_b = 10000;
        config_.enable_rtcp = false;  // Disable RTCP for simpler testing
    }
    
    E2EConfig config_;
};

// ============================================================================
// E2E Tests
// ============================================================================

TEST_F(E2ETestFixture, AudioCall) {
    E2ETest tester;
    
    // Initialize with config
    if (!tester.Initialize(config_)) {
        FAIL() << "Failed to initialize tester";
        return;
    }
    
    auto result = tester.TestAudioCall(3);
    
    std::cout << "Audio Call Result:" << std::endl;
    std::cout << "  Success: " << (result.success ? "YES" : "NO") << std::endl;
    std::cout << "  Packets Sent: " << result.packets_sent << std::endl;
    std::cout << "  Packets Received: " << result.packets_received << std::endl;
    std::cout << "  Loss Rate: " << (result.loss_rate * 100) << "%" << std::endl;
    
    if (!result.success) {
        std::cout << "  Error: " << result.error_message << std::endl;
    }
    
    // Verify we actually sent packets
    EXPECT_GT(result.packets_sent, 0);
    
    // Verify we received some packets (allow some loss)
    EXPECT_GT(result.packets_received, 0);
    
    // Success criteria: less than 50% loss (local loopback should be reliable)
    EXPECT_LT(result.loss_rate, 0.5);
}

TEST_F(E2ETestFixture, VideoCall) {
    E2ETest tester;
    
    // Initialize with config
    if (!tester.Initialize(config_)) {
        FAIL() << "Failed to initialize tester";
        return;
    }
    
    auto result = tester.TestVideoCall(3);
    
    std::cout << "Video Call Result:" << std::endl;
    std::cout << "  Success: " << (result.success ? "YES" : "NO") << std::endl;
    std::cout << "  Packets Sent: " << result.packets_sent << std::endl;
    std::cout << "  Packets Received: " << result.packets_received << std::endl;
    std::cout << "  Loss Rate: " << (result.loss_rate * 100) << "%" << std::endl;
    
    if (!result.success) {
        std::cout << "  Error: " << result.error_message << std::endl;
    }
    
    // Verify we actually sent packets
    EXPECT_GT(result.packets_sent, 0);
    
    // Verify we received some packets
    EXPECT_GT(result.packets_received, 0);
    
    // Success criteria: less than 50% loss
    EXPECT_LT(result.loss_rate, 0.5);
}

TEST_F(E2ETestFixture, Loopback) {
    E2ETest tester;
    
    // Initialize with config
    if (!tester.Initialize(config_)) {
        FAIL() << "Failed to initialize tester";
        return;
    }
    
    auto result = tester.TestLoopback(3);
    
    std::cout << "Loopback Result:" << std::endl;
    std::cout << "  Success: " << (result.success ? "YES" : "NO") << std::endl;
    std::cout << "  Packets Sent: " << result.packets_sent << std::endl;
    std::cout << "  Packets Received: " << result.packets_received << std::endl;
    std::cout << "  Loss Rate: " << (result.loss_rate * 100) << "%" << std::endl;
    
    if (!result.success) {
        std::cout << "  Error: " << result.error_message << std::endl;
    }
    
    // Verify we actually sent packets
    EXPECT_GT(result.packets_sent, 0);
    
    // Verify we received packets (loopback should have very low loss)
    EXPECT_GT(result.packets_received, 0);
    
    // Loopback should have minimal loss
    EXPECT_LT(result.loss_rate, 0.5);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
