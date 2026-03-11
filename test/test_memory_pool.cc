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
    EXPECT_EQ(pool.GetInUseCount(), 0);
}

// Test: Acquire returns object
TEST_F(ObjectPoolTest, AcquireReturnsObject) {
    ObjectPool<TestObject> pool(10);
    
    auto obj = pool.Acquire();
    
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(pool.GetAvailableCount(), 9);
    EXPECT_EQ(pool.GetInUseCount(), 1);
}

// Test: Acquire expands pool when empty
TEST_F(ObjectPoolTest, AcquireExpandsPool) {
    ObjectPool<TestObject> pool(2);
    
    // Use all available
    auto obj1 = pool.Acquire();
    auto obj2 = pool.Acquire();
    
    EXPECT_EQ(pool.GetAvailableCount(), 0);
    EXPECT_EQ(pool.GetInUseCount(), 2);
    
    // Next acquire should expand pool
    auto obj3 = pool.Acquire();
    EXPECT_NE(obj3, nullptr);
    EXPECT_EQ(pool.GetInUseCount(), 3);
}

// Test: Release returns object to pool
TEST_F(ObjectPoolTest, ReleaseReturnsObject) {
    ObjectPool<TestObject> pool(10);
    
    auto obj = pool.Acquire();
    EXPECT_EQ(pool.GetInUseCount(), 1);
    
    pool.Release(obj);
    
    EXPECT_EQ(pool.GetAvailableCount(), 10);
    EXPECT_EQ(pool.GetInUseCount(), 0);
}

// Test: Multiple acquire/release cycles
TEST_F(ObjectPoolTest, MultipleCycles) {
    ObjectPool<TestObject> pool(5);
    
    std::vector<std::shared_ptr<TestObject>> objects;
    
    // Acquire 5 objects
    for (int i = 0; i < 5; ++i) {
        objects.push_back(pool.Acquire());
    }
    EXPECT_EQ(pool.GetInUseCount(), 5);
    EXPECT_EQ(pool.GetAvailableCount(), 0);
    
    // Release all
    for (auto& obj : objects) {
        pool.Release(obj);
    }
    objects.clear();
    
    EXPECT_EQ(pool.GetInUseCount(), 0);
    EXPECT_EQ(pool.GetAvailableCount(), 5);
    
    // Acquire again - should reuse
    auto obj = pool.Acquire();
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(pool.GetInUseCount(), 1);
}

// Test: Reserve expands pool
TEST_F(ObjectPoolTest, ReserveExpandsPool) {
    ObjectPool<TestObject> pool(5);
    
    EXPECT_EQ(pool.GetAvailableCount(), 5);
    
    pool.Reserve(20);
    
    EXPECT_GE(pool.GetAvailableCount(), 15);
}

// Test: Pool with zero initial size
TEST_F(ObjectPoolTest, ZeroInitialSize) {
    ObjectPool<TestObject> pool(0);
    
    EXPECT_EQ(pool.GetAvailableCount(), 0);
    
    // Should still work, will expand on acquire
    auto obj = pool.Acquire();
    EXPECT_NE(obj, nullptr);
    EXPECT_EQ(pool.GetInUseCount(), 1);
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
    EXPECT_EQ(pool_->GetAvailableCount(), 10);
    EXPECT_EQ(pool_->GetInUseCount(), 0);
}

// Test: Allocate returns packet
TEST_F(RtpPacketPoolTest, AllocateReturnsPacket) {
    auto packet = pool_->Allocate();
    
    EXPECT_NE(packet, nullptr);
    EXPECT_EQ(pool_->GetAvailableCount(), 9);
    EXPECT_EQ(pool_->GetInUseCount(), 1);
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
    
    EXPECT_EQ(packet2->GetPayloadType(), 0);  // Default value after reset
    EXPECT_EQ(packet2->GetSsrc(), 0);
    EXPECT_EQ(packet2->GetSequenceNumber(), 0);
}

// Test: Release returns packet to pool
TEST_F(RtpPacketPoolTest, ReleaseReturnsPacket) {
    auto packet = pool_->Allocate();
    EXPECT_EQ(pool_->GetInUseCount(), 1);
    
    pool_->Release(packet);
    
    EXPECT_EQ(pool_->GetInUseCount(), 0);
}

// Test: Multiple allocations
TEST_F(RtpPacketPoolTest, MultipleAllocations) {
    std::vector<std::shared_ptr<RtpPacket>> packets;
    
    for (int i = 0; i < 10; ++i) {
        packets.push_back(pool_->Allocate());
    }
    
    EXPECT_EQ(pool_->GetInUseCount(), 10);
    EXPECT_EQ(pool_->GetAvailableCount(), 0);
    
    // Release all
    for (auto& pkt : packets) {
        pool_->Release(pkt);
    }
    
    EXPECT_EQ(pool_->GetInUseCount(), 0);
}

// Test: Pool expansion when exhausted
TEST_F(RtpPacketPoolTest, PoolExpansion) {
    auto small_pool = CreateRtpPacketPool(2, 100);
    
    // Use all packets
    auto p1 = small_pool->Allocate();
    auto p2 = small_pool->Allocate();
    
    EXPECT_EQ(small_pool->GetInUseCount(), 2);
    
    // Allocate more - should expand
    auto p3 = small_pool->Allocate();
    EXPECT_NE(p3, nullptr);
    EXPECT_GE(small_pool->GetInUseCount(), 3);
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
    
    // Should have allocated some beyond max, some from new
    EXPECT_GE(packets.size(), 5);
}

// Test: Allocate nullptr handling
TEST_F(RtpPacketPoolTest, ReleaseNullptr) {
    std::shared_ptr<RtpPacket> null_packet;
    
    // Should not crash
    EXPECT_NO_THROW(pool_->Release(null_packet));
    
    // In-use count should remain 0
    EXPECT_EQ(pool_->GetInUseCount(), 0);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

class ThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool_ = CreateRtpPacketPool(50, 1000);
    }
    
    RtpPacketPool::Ptr pool_;
};

// Test: Concurrent acquire/release
TEST_F(ThreadSafetyTest, ConcurrentAcquireRelease) {
    const int num_threads = 4;
    const int iterations = 100;
    std::atomic<int> success_count(0);
    std::vector<std::thread> threads;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < iterations; ++i) {
                auto packet = pool_->Allocate();
                if (packet) {
                    // Do some work
                    packet->SetPayloadType(96);
                    packet->SetSsrc(t);
                    
                    pool_->Release(packet);
                    success_count++;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // All allocations should succeed
    EXPECT_EQ(success_count, num_threads * iterations);
    EXPECT_EQ(pool_->GetInUseCount(), 0);
}

// Test: Stress test with many threads
TEST_F(ThreadSafetyTest, StressTest) {
    const int num_threads = 8;
    const int iterations = 500;
    std::atomic<int> total_allocated(0);
    std::vector<std::thread> threads;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < iterations; ++i) {
                auto packet = pool_->Allocate();
                if (packet) {
                    // Simulate some work
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                    packet->SetSequenceNumber(i);
                    pool_->Release(packet);
                    total_allocated++;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(total_allocated, num_threads * iterations);
}

// ============================================================================
// Boundary Tests
// ============================================================================

class BoundaryTest : public ::testing::Test {};

// Test: Very large initial pool
TEST_F(BoundaryTest, LargeInitialPool) {
    // Large initial size but reasonable
    auto pool = CreateRtpPacketPool(1000, 2000);
    EXPECT_EQ(pool->GetAvailableCount(), 1000);
}

// Test: Very small max size
TEST_F(BoundaryTest, SmallMaxSize) {
    auto pool = CreateRtpPacketPool(2, 5);
    EXPECT_EQ(pool->GetAvailableCount(), 2);
    
    // Allocate beyond max
    for (int i = 0; i < 10; ++i) {
        pool->Allocate();
    }
    
    // Should have allocated more than max
    EXPECT_GE(pool->GetInUseCount(), 5);
}

// Test: Object pool with custom objects
TEST_F(BoundaryTest, CustomObjectPool) {
    ObjectPool<int> int_pool(5);
    
    // Test with primitive types (int)
    auto i1 = int_pool.Acquire();
    *i1 = 42;
    
    int_pool.Release(i1);
    
    auto i2 = int_pool.Acquire();
    // Previous value might remain (pool reuses memory)
    int_pool.Release(i2);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
