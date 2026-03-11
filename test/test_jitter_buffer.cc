/**
 * @file test_jitter_buffer.cc
 * @brief Unit tests for JitterBuffer
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>
#include <atomic>

#include "minirtc/jitter_buffer.h"
#include "minirtc/transport/rtp_packet.h"

using namespace minirtc;

// ============================================================================
// JitterBuffer Tests
// ============================================================================

class JitterBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        buffer_ = CreateJitterBuffer(JitterBufferMode::kPassthrough);
        config_.mode = JitterBufferMode::kPassthrough;
    }
    
    JitterBufferConfig config_;
    IJitterBuffer::Ptr buffer_;
};

TEST_F(JitterBufferTest, Initialize) {
    EXPECT_TRUE(buffer_->Initialize(config_));
}

TEST_F(JitterBufferTest, PassthroughModeBasic) {
    buffer_->Initialize(config_);
    
    // Add packet
    auto packet = std::make_shared<RtpPacket>(96, 1000, 1);
    packet->SetSsrc(0x12345678);
    buffer_->AddPacket(packet);
    
    // Get packet immediately (passthrough)
    auto received = buffer_->GetPacket(100);
    EXPECT_NE(received, nullptr);
    EXPECT_EQ(received->GetTimestamp(), 1000u);
    EXPECT_EQ(received->GetSequenceNumber(), 1u);
}

TEST_F(JitterBufferTest, PassthroughModeMultiplePackets) {
    buffer_->Initialize(config_);
    
    // Add multiple packets
    for (int i = 0; i < 5; ++i) {
        auto packet = std::make_shared<RtpPacket>(96, 1000 + i * 3000, static_cast<uint16_t>(i + 1));
        buffer_->AddPacket(packet);
    }
    
    // Get all packets
    int count = 0;
    while (true) {
        auto packet = buffer_->GetPacket(100);
        if (!packet) break;
        count++;
    }
    EXPECT_EQ(count, 5);
}

TEST_F(JitterBufferTest, StatsTest) {
    buffer_->Initialize(config_);
    
    auto stats = buffer_->GetStats();
    EXPECT_EQ(stats.packets_in, 0u);
    EXPECT_EQ(stats.packets_out, 0u);
    EXPECT_EQ(stats.packets_dropped, 0u);
    
    // Add 3 packets
    for (int i = 0; i < 3; ++i) {
        auto packet = std::make_shared<RtpPacket>(96, 1000, static_cast<uint16_t>(i + 1));
        buffer_->AddPacket(packet);
    }
    
    // Get 2 packets
    buffer_->GetPacket(100);
    buffer_->GetPacket(100);
    
    stats = buffer_->GetStats();
    EXPECT_EQ(stats.packets_in, 3u);
    EXPECT_EQ(stats.packets_out, 2u);
}

TEST_F(JitterBufferTest, NullPacketHandling) {
    buffer_->Initialize(config_);
    
    // Add null packet - should be dropped
    buffer_->AddPacket(nullptr);
    
    auto stats = buffer_->GetStats();
    EXPECT_EQ(stats.packets_dropped, 0u);  // Null is just ignored
    
    // Get timeout
    auto packet = buffer_->GetPacket(100);
    EXPECT_EQ(packet, nullptr);
}

TEST_F(JitterBufferTest, StopTest) {
    buffer_->Initialize(config_);
    
    auto packet = std::make_shared<RtpPacket>(96, 1000, 1);
    buffer_->AddPacket(packet);
    
    buffer_->Stop();
    
    // After stop, packet should be dropped
    auto stats = buffer_->GetStats();
    EXPECT_EQ(stats.packets_in, 1u);
}

TEST_F(JitterBufferTest, ExtendedStatsTest) {
    buffer_->Initialize(config_);
    
    // Add some packets
    for (int i = 0; i < 3; ++i) {
        auto packet = std::make_shared<RtpPacket>(96, 1000 + i * 3000, static_cast<uint16_t>(i + 1));
        buffer_->AddPacket(packet);
    }
    
    auto ex_stats = buffer_->GetStatsEx();
    EXPECT_EQ(ex_stats.packets_in, 3u);
    EXPECT_GE(ex_stats.buffer_size, 0u);
}

// ============================================================================
// Fixed Delay Mode Tests
// ============================================================================

class FixedJitterBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        buffer_ = CreateJitterBuffer(JitterBufferMode::kFixed);
        config_.mode = JitterBufferMode::kFixed;
        config_.fixed_delay_ms = 30;
        config_.max_delay_ms = 200;
        config_.min_delay_ms = 20;
    }
    
    JitterBufferConfig config_;
    IJitterBuffer::Ptr buffer_;
};

TEST_F(FixedJitterBufferTest, FixedDelayMode) {
    buffer_->Initialize(config_);
    
    // Add multiple packets
    for (int i = 0; i < 5; ++i) {
        auto packet = std::make_shared<RtpPacket>(96, 1000 + i * 3000, static_cast<uint16_t>(i + 1));
        buffer_->AddPacket(packet);
    }
    
    // Wait longer than max delay
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // Get all packets - should be delivered in order after delay
    int count = 0;
    while (true) {
        auto packet = buffer_->GetPacket(100);
        if (!packet) break;
        count++;
    }
    EXPECT_EQ(count, 5);
}

TEST_F(FixedJitterBufferTest, SetFixedDelay) {
    buffer_->Initialize(config_);
    
    // Change delay
    buffer_->SetFixedDelay(100);
    
    auto ex_stats = buffer_->GetStatsEx();
    EXPECT_EQ(ex_stats.current_delay_ms, 100);
}

// ============================================================================
// Adaptive Mode Tests
// ============================================================================

class AdaptiveJitterBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        buffer_ = CreateJitterBuffer(JitterBufferMode::kAdaptive);
        config_.mode = JitterBufferMode::kAdaptive;
        config_.fixed_delay_ms = 30;
        config_.max_delay_ms = 200;
        config_.min_delay_ms = 20;
    }
    
    JitterBufferConfig config_;
    IJitterBuffer::Ptr buffer_;
};

TEST_F(AdaptiveJitterBufferTest, AdaptiveModeBasic) {
    buffer_->Initialize(config_);
    
    // Add packet
    auto packet = std::make_shared<RtpPacket>(96, 1000, 1);
    buffer_->AddPacket(packet);
    
    // Adaptive mode has minimum delay, wait and try
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto received = buffer_->GetPacket(100);
    // May or may not get packet depending on adaptive delay
    // Just verify no crash
}

TEST_F(AdaptiveJitterBufferTest, SetMaxDelay) {
    buffer_->Initialize(config_);
    
    buffer_->SetMaxDelay(300);
    
    auto ex_stats = buffer_->GetStatsEx();
    // Initial delay should be at least min_delay_ms
    EXPECT_GE(ex_stats.current_delay_ms, 20);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

class JitterBufferThreadTest : public ::testing::Test {
protected:
    void SetUp() override {
        buffer_ = CreateJitterBuffer(JitterBufferMode::kPassthrough);
        config_.mode = JitterBufferMode::kPassthrough;
        buffer_->Initialize(config_);
    }
    
    JitterBufferConfig config_;
    IJitterBuffer::Ptr buffer_;
};

TEST_F(JitterBufferThreadTest, ConcurrentAddGet) {
    std::atomic<int> add_count{0};
    std::atomic<int> get_count{0};
    
    // Producer thread
    std::thread producer([this, &add_count]() {
        for (int i = 0; i < 100; ++i) {
            auto packet = std::make_shared<RtpPacket>(96, 1000 + i * 3000, static_cast<uint16_t>(i));
            buffer_->AddPacket(packet);
            add_count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Consumer thread
    std::thread consumer([this, &get_count]() {
        while (get_count < 100) {
            auto packet = buffer_->GetPacket(50);
            if (packet) {
                get_count++;
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(add_count, 100);
    EXPECT_EQ(get_count, 100);
}

TEST_F(JitterBufferThreadTest, MultipleProducersConsumers) {
    std::atomic<int> total_added{0};
    std::atomic<int> total_received{0};
    
    // Multiple producers
    std::vector<std::thread> producers;
    for (int t = 0; t < 3; ++t) {
        producers.emplace_back([this, &total_added, t]() {
            for (int i = 0; i < 50; ++i) {
                auto packet = std::make_shared<RtpPacket>(96, 1000 + i * 3000, 
                    static_cast<uint16_t>(t * 50 + i));
                buffer_->AddPacket(packet);
                total_added++;
            }
        });
    }
    
    // Multiple consumers
    std::vector<std::thread> consumers;
    for (int t = 0; t < 3; ++t) {
        consumers.emplace_back([this, &total_received, t]() {
            while (total_received < 150) {
                auto packet = buffer_->GetPacket(10);
                if (packet) {
                    total_received++;
                }
            }
        });
    }
    
    for (auto& p : producers) p.join();
    for (auto& c : consumers) c.join();
    
    EXPECT_EQ(total_added, 150);
    EXPECT_EQ(total_received, 150);
}

TEST_F(JitterBufferThreadTest, StressTest) {
    std::atomic<bool> running{true};
    std::atomic<int> total{0};
    
    // High frequency add/get
    std::thread adder([this, &running, &total]() {
        for (int i = 0; running.load(); ++i) {
            auto packet = std::make_shared<RtpPacket>(96, i, static_cast<uint16_t>(i % 65536));
            buffer_->AddPacket(packet);
            total++;
        }
    });
    
    std::thread getter([this, &running, &total]() {
        int count = 0;
        while (running.load() || count < total.load()) {
            auto packet = buffer_->GetPacket(1);
            if (packet) {
                count++;
            }
        }
    });
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    running = false;
    
    adder.join();
    getter.join();
    
    // Both should complete without crash
    EXPECT_GT(total.load(), 0);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
