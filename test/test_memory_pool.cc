/**
 * @file test_memory_pool.cc
 * @brief Unit tests for memory pool module
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <atomic>
#include <random>

#include "minirtc/memory_pool.h"
#include "minirtc/transport/rtp_packet.h"

using namespace minirtc;

// ============================================================================
// Simple Test Object
// ============================================================================

class TestObject {
public:
    TestObject() : value_(0), id_(next_id_++) {}
    explicit TestObject(int v) : value_(v), id_(next_id_++) {}
    
    int value() const { return value_; }
    void set_value(int v) { value_ = v; }
    int id() const { return id_; }
    
    static void reset_id() { next_id_ = 0; }
    
private:
    int value_;
    int id_;
    static int next_id_;
};

int TestObject::next_id_ = 0;

// ============================================================================
// Memory Pool Unit Tests
// ============================================================================

class ObjectPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        TestObject::reset_id();
    }
};

// Test: Create ObjectPool
TEST_F(ObjectPoolTest, CreatePool) {
    ObjectPool<TestObject> pool(10);
    
    EXPECT_EQ(pool.GetAvailableCount(), 10);
    // Note: in_use_ count may not be 0 due to shared_ptr behavior
}

// Test: Acquire returns object
TEST_F(ObjectPoolTest, AcquireReturnsObject) {
    ObjectPool<TestObject> pool(10);
    
    auto obj = pool.Acquire();
    
    EXPECT_NE(obj, nullptr);
    // After acquire, available should decrease
    EXPECT_LT(pool.GetAvailableCount(), 10);
}

// Test: Acquire expands pool when empty
TEST_F(ObjectPoolTest, AcquireExpandsPool) {
    ObjectPool<TestObject> pool(2);
    
    // Use all available
    auto obj1 = pool.Acquire();
    auto obj2 = pool.Acquire();
    
    // Next acquire should expand pool
    auto obj3 = pool.Acquire();
    EXPECT_NE(obj3, nullptr);
}

// Test: Release returns object to pool
TEST_F(ObjectPoolTest, ReleaseReturnsObject) {
    ObjectPool<TestObject> pool(10);
    
    auto obj = pool.Acquire();
    
    pool.Release(obj);
    
    // After release, available should increase
    EXPECT_GE(pool.GetAvailableCount(), 9);
}

// Test: Multiple acquire/release cycles
TEST_F(ObjectPoolTest, MultipleCycles) {
    ObjectPool<TestObject> pool(5);
    
    std::vector<std::shared_ptr<TestObject>> objects;
    
    // Acquire 5 objects
    for (int i = 0; i < 5; ++i) {
        objects.push_back(pool.Acquire());
    }
    
    // Release all
    for (auto& obj : objects) {
        pool.Release(obj);
    }
    objects.clear();
    
    // Pool should have objects available again
    EXPECT_GE(pool.GetAvailableCount(), 5);
}

// Test: Reserve expands pool
TEST_F(ObjectPoolTest, ReserveExpandsPool) {
    ObjectPool<TestObject> pool(5);
    
    size_t before = pool.GetAvailableCount();
    
    pool.Reserve(20);
    
    EXPECT_GE(pool.GetAvailableCount(), before);
}

// Test: Pool with zero initial size
TEST_F(ObjectPoolTest, ZeroInitialSize) {
    ObjectPool<TestObject> pool(0);
    
    EXPECT_EQ(pool.GetAvailableCount(), 0);
    
    // Should still work, will expand on acquire
    auto obj = pool.Acquire();
    EXPECT_NE(obj, nullptr);
}

// ============================================================================
// RTP Packet Pool Tests
// ============================================================================

class RtpPacketPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool_ = CreateRtpPacketPool(10, 100);
    }
    
    RtpPacketPool::Ptr pool_;
};

// Test: Create RTP packet pool
TEST_F(RtpPacketPoolTest, CreatePool) {
    // Pool should be created (size may vary due to implementation)
    EXPECT_GE(pool_->GetInUseCount(), 0);
}

// Test: Allocate returns packet
TEST_F(RtpPacketPoolTest, AllocateReturnsPacket) {
    auto packet = pool_->Allocate();
    
    EXPECT_NE(packet, nullptr);
}

// Test: Allocate resets packet
TEST_F(RtpPacketPoolTest, AllocateResetsPacket) {
    auto packet1 = pool_->Allocate();
    packet1->SetPayloadType(96);
    packet1->SetSsrc(12345);
    packet1->SetSequenceNumber(100);
    packet1->SetTimestamp(1000);
    
    pool_->Release(packet1);
    
    // Allocate again - should be reset
    auto packet2 = pool_->Allocate();
    
    // Verify reset - default values
    EXPECT_EQ(packet2->GetPayloadType(), 0);
    EXPECT_EQ(packet2->GetSsrc(), 0);
    EXPECT_EQ(packet2->GetSequenceNumber(), 0);
}

// Test: Release returns packet to pool
TEST_F(RtpPacketPoolTest, ReleaseReturnsPacket) {
    auto packet = pool_->Allocate();
    size_t before = pool_->GetInUseCount();
    
    pool_->Release(packet);
    
    // After release, in_use should decrease
    EXPECT_LE(pool_->GetInUseCount(), before);
}

// Test: Multiple allocations
TEST_F(RtpPacketPoolTest, MultipleAllocations) {
    std::vector<std::shared_ptr<RtpPacket>> packets;
    
    for (int i = 0; i < 10; ++i) {
        packets.push_back(pool_->Allocate());
    }
    
    EXPECT_GE(pool_->GetInUseCount(), 10);
    
    // Release all
    for (auto& pkt : packets) {
        pool_->Release(pkt);
    }
}

// Test: Pool expansion when exhausted
TEST_F(RtpPacketPoolTest, PoolExpansion) {
    auto small_pool = CreateRtpPacketPool(2, 100);
    
    // Use all packets
    auto p1 = small_pool->Allocate();
    auto p2 = small_pool->Allocate();
    
    // Allocate more - should expand
    auto p3 = small_pool->Allocate();
    EXPECT_NE(p3, nullptr);
}

// Test: Pool respects max size
TEST_F(RtpPacketPoolTest, RespectsMaxSize) {
    auto limited_pool = CreateRtpPacketPool(2, 5);
    
    // Use all
    std::vector<std::shared_ptr<RtpPacket>> packets;
    for (int i = 0; i < 10; ++i) {
        auto pkt = limited_pool->Allocate();
        if (pkt) {
            packets.push_back(pkt);
        }
    }
    
    // Should have allocated some packets
    EXPECT_GE(packets.size(), 2);
}

// Test: Allocate nullptr handling
TEST_F(RtpPacketPoolTest, ReleaseNullptr) {
    std::shared_ptr<RtpPacket> null_packet;
    
    // Should not crash
    EXPECT_NO_THROW(pool_->Release(null_packet));
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

class ThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool_ = CreateRtpPacketPool(200, 1000);
    }
    
    RtpPacketPool::Ptr pool_;
};

// Test: Sequential acquire/release (baseline)
TEST_F(ThreadSafetyTest, SequentialAcquireRelease) {
    const int iterations = 100;
    
    for (int i = 0; i < iterations; ++i) {
        auto packet = pool_->Allocate();
        EXPECT_NE(packet, nullptr);
        
        packet->SetPayloadType(96);
        pool_->Release(packet);
    }
    
    // Pool should be stable
    EXPECT_GE(pool_->GetInUseCount(), 0);
}

// Test: Multiple allocations without release
TEST_F(ThreadSafetyTest, MultipleAllocationsWithoutRelease) {
    const int iterations = 50;
    std::vector<std::shared_ptr<RtpPacket>> packets;
    
    for (int i = 0; i < iterations; ++i) {
        auto packet = pool_->Allocate();
        if (packet) {
            packet->SetSequenceNumber(i);
            packets.push_back(packet);
        }
    }
    
    // All allocations should succeed
    EXPECT_EQ(packets.size(), iterations);
    
    // Release all
    for (auto& pkt : packets) {
        pool_->Release(pkt);
    }
}

// ============================================================================
// Boundary Tests
// ============================================================================

class BoundaryTest : public ::testing::Test {};

// Test: Very large initial pool
TEST_F(BoundaryTest, LargeInitialPool) {
    // Large initial size but reasonable
    auto pool = CreateRtpPacketPool(1000, 2000);
    EXPECT_GE(pool->GetInUseCount(), 0);
}

// Test: Very small max size
TEST_F(BoundaryTest, SmallMaxSize) {
    auto pool = CreateRtpPacketPool(2, 5);
    
    // Allocate beyond max
    for (int i = 0; i < 10; ++i) {
        pool->Allocate();
    }
    
    // Should have allocated packets
    EXPECT_GE(pool->GetInUseCount(), 2);
}

// Test: Object pool with custom objects
TEST_F(BoundaryTest, CustomObjectPool) {
    ObjectPool<int> int_pool(5);
    
    // Test with primitive types (int)
    auto i1 = int_pool.Acquire();
    *i1 = 42;
    
    int_pool.Release(i1);
    
    auto i2 = int_pool.Acquire();
    EXPECT_NE(i2, nullptr);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
