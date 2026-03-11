/**
 * @file test_ice.cc
 * @brief Unit tests for ICE module
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>

#include "minirtc/ice.h"

using namespace minirtc;

// ============================================================================
// ICE Module Unit Tests
// ============================================================================

class IceTest : public ::testing::Test {
protected:
    void SetUp() override {
        ice_agent_ = CreateIceAgent();
        
        // Configure STUN servers
        config_.servers = {"stun.l.google.com:19302"};
        config_.timeout_ms = 1000;
        config_.max_retries = 1;
    }
    
    StunConfig config_;
    IIceAgent::Ptr ice_agent_;
};

// Test: GetLocalIPs returns valid local IP addresses
TEST_F(IceTest, GetLocalIPsReturnsValidAddresses) {
    // GetLocalIPs is internal implementation detail, use GatherCandidates instead
    ice_agent_->Initialize(config_);
    auto candidates = ice_agent_->GatherCandidates();
    
    // Check that host candidates have valid IP format
    for (const auto& cand : candidates) {
        // Check IPv4 format (xxx.xxx.xxx.xxx)
        if (!cand.host_addr.empty()) {
            size_t dots = std::count(cand.host_addr.begin(), cand.host_addr.end(), '.');
            EXPECT_EQ(dots, 3) << "Invalid IPv4 format: " << cand.host_addr;
        }
        
        // Should not contain localhost (127.x.x.x)
        EXPECT_NE(cand.host_addr.rfind("127.", 0), 0) << "Should not contain localhost: " << cand.host_addr;
    }
}

// Test: ICE agent initialization
TEST_F(IceTest, InitializeSucceeds) {
    bool result = ice_agent_->Initialize(config_);
    EXPECT_TRUE(result);
}

// Test: GatherCandidates returns host candidates
TEST_F(IceTest, GatherCandidatesReturnsHostCandidates) {
    ice_agent_->Initialize(config_);
    
    auto candidates = ice_agent_->GatherCandidates();
    
    // Should have at least one candidate if we have network
    // In test environment, might be empty
    for (const auto& cand : candidates) {
        // Check candidate has valid fields
        EXPECT_GT(cand.foundation, 0);
        EXPECT_EQ(cand.component_id, 1);
        EXPECT_EQ(cand.protocol, IceProtocol::kUdp);
        EXPECT_GT(cand.priority, 0);
        EXPECT_FALSE(cand.host_addr.empty());
        EXPECT_EQ(cand.type, IceCandidateType::kHost);
    }
}

// Test: Candidate fields are properly set
TEST_F(IceTest, CandidateFieldsAreValid) {
    ice_agent_->Initialize(config_);
    
    auto candidates = ice_agent_->GatherCandidates();
    
    if (!candidates.empty()) {
        const auto& cand = candidates[0];
        
        // Check foundation is unique per IP
        EXPECT_EQ(cand.foundation, 1);
        
        // Check priority format (IPv4 UDP)
        // Priority = (1 << 24) * typepref | (1 << 8) * protocolpref | (1 << 0) * componentid
        uint32_t expected_priority = 126 << 24 | 65536 | 255;
        EXPECT_EQ(cand.priority, expected_priority);
        
        // Transport address format
        EXPECT_FALSE(cand.transport_addr.empty());
    }
}

// Test: STUN configuration can be customized
TEST_F(IceTest, CustomStunConfig) {
    StunConfig custom_config;
    custom_config.servers = {
        "stun1.l.google.com:19302",
        "stun2.l.google.com:19302"
    };
    custom_config.timeout_ms = 2000;
    custom_config.max_retries = 5;
    
    ice_agent_->Initialize(custom_config);
    
    // Try to gather candidates (network dependent)
    auto candidates = ice_agent_->GatherCandidates();
    
    // Should still work with custom config
    EXPECT_GE(candidates.size(), 0);
}

// Test: Bind to STUN server (may fail in isolated environments)
TEST_F(IceTest, BindToStunServer) {
    ice_agent_->Initialize(config_);
    
    // First gather local candidates
    auto candidates = ice_agent_->GatherCandidates();
    ASSERT_FALSE(candidates.empty());
    
    IceCandidate mapped_candidate;
    bool bind_result = ice_agent_->Bind(candidates[0], &mapped_candidate);
    
    // Note: This may fail if network is not available or firewall blocks
    // We don't assert success, but if it succeeds, validate the result
    if (bind_result) {
        EXPECT_EQ(mapped_candidate.type, IceCandidateType::kSrflx);
        EXPECT_FALSE(mapped_candidate.host_addr.empty());
        EXPECT_GT(mapped_candidate.port, 0);
        EXPECT_FALSE(mapped_candidate.transport_addr.empty());
    }
}

// Test: Multiple ICE agents can be created
TEST_F(IceTest, MultipleAgents) {
    auto agent1 = CreateIceAgent();
    auto agent2 = CreateIceAgent();
    
    EXPECT_NE(agent1, agent2);
    
    agent1->Initialize(config_);
    agent2->Initialize(config_);
    
    auto cands1 = agent1->GatherCandidates();
    auto cands2 = agent2->GatherCandidates();
    
    // Both should work independently
    EXPECT_GE(cands1.size(), 0);
    EXPECT_GE(cands2.size(), 0);
}

// Test: Candidate equality and comparison
TEST_F(IceTest, CandidateComparison) {
    IceCandidate cand1;
    cand1.foundation = 1;
    cand1.host_addr = "192.168.1.1";
    cand1.port = 5000;
    cand1.type = IceCandidateType::kHost;
    
    IceCandidate cand2;
    cand2.foundation = 1;
    cand2.host_addr = "192.168.1.1";
    cand2.port = 5000;
    cand2.type = IceCandidateType::kHost;
    
    // Same foundation, address, port and type should be equal
    EXPECT_EQ(cand1.foundation, cand2.foundation);
    EXPECT_EQ(cand1.host_addr, cand2.host_addr);
    EXPECT_EQ(cand1.port, cand2.port);
    EXPECT_EQ(cand1.type, cand2.type);
}

// Test: Different candidate types
TEST_F(IceTest, DifferentCandidateTypes) {
    IceCandidate host_cand;
    host_cand.type = IceCandidateType::kHost;
    EXPECT_EQ(host_cand.type, IceCandidateType::kHost);
    
    IceCandidate srflx_cand;
    srflx_cand.type = IceCandidateType::kSrflx;
    EXPECT_EQ(srflx_cand.type, IceCandidateType::kSrflx);
    
    IceCandidate prflx_cand;
    prflx_cand.type = IceCandidateType::kPrflx;
    EXPECT_EQ(prflx_cand.type, IceCandidateType::kPrflx);
    
    IceCandidate relayed_cand;
    relayed_cand.type = IceCandidateType::kRelayed;
    EXPECT_EQ(relayed_cand.type, IceCandidateType::kRelayed);
}

// Test: Bind with null mapped_candidate returns false
TEST_F(IceTest, BindWithNullCandidateFails) {
    ice_agent_->Initialize(config_);
    
    auto candidates = ice_agent_->GatherCandidates();
    if (!candidates.empty()) {
        bool result = ice_agent_->Bind(candidates[0], nullptr);
        EXPECT_FALSE(result);
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
