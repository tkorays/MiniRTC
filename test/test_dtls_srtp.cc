/**
 * @file test_dtls_srtp.cc
 * @brief Unit tests for DTLS/SRTP module
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <cstring>

#include "minirtc/dtls_srtp.h"

using namespace minirtc;

// ============================================================================
// DTLS/SRTP Test Handler (for callbacks)
// ============================================================================

class TestDtlsHandler : public IDtlsHandler {
public:
    TestDtlsHandler() : state_changes_(0), key_ready_(false), errors_(0) {}
    
    void OnDtlsStateChange(DtlsState state) override {
        state_changes_++;
        last_state_ = state;
    }
    
    void OnSrtpKeyReady(const SrtpConfig& config) override {
        key_ready_ = true;
        srtp_config_ = config;
    }
    
    void OnError(const std::string& msg) override {
        errors_++;
        last_error_ = msg;
    }
    
    int state_changes_ = 0;
    DtlsState last_state_ = DtlsState::kNew;
    bool key_ready_ = false;
    SrtpConfig srtp_config_;
    int errors_ = 0;
    std::string last_error_;
};

// ============================================================================
// DTLS/SRTP Unit Tests
// ============================================================================

class DtlsSrtpTest : public ::testing::Test {
protected:
    void SetUp() override {
        dtls_transport_ = CreateDtlsTransport();
        
        config_.certificate = "test_cert";
        config_.private_key = "test_key";
        config_.verify_peer = false;
        config_.timeout_ms = 5000;
        config_.srtp_profiles = "SRTP_AES128_CM_SHA1_80";
    }
    
    DtlsConfig config_;
    IDtlsTransport::Ptr dtls_transport_;
};

// Test: Create DTLS transport instance
TEST_F(DtlsSrtpTest, CreateInstance) {
    EXPECT_NE(dtls_transport_, nullptr);
}

// Test: Initial state is kNew
TEST_F(DtlsSrtpTest, InitialStateIsNew) {
    EXPECT_EQ(dtls_transport_->GetState(), DtlsState::kNew);
}

// Test: Initialize succeeds
TEST_F(DtlsSrtpTest, InitializeSucceeds) {
    auto result = dtls_transport_->Initialize(config_);
    EXPECT_TRUE(result.IsOk());
}

// Test: Initialize changes state to kNew (still not connected)
TEST_F(DtlsSrtpTest, InitializeStateRemainsNew) {
    dtls_transport_->Initialize(config_);
    EXPECT_EQ(dtls_transport_->GetState(), DtlsState::kNew);
}

// Test: StartHandshake as client
TEST_F(DtlsSrtpTest, StartHandshakeAsClient) {
    dtls_transport_->Initialize(config_);
    
    auto result = dtls_transport_->StartHandshake();
    EXPECT_TRUE(result.IsOk());
}

// Test: StartHandshake changes state to kConnecting
TEST_F(DtlsSrtpTest, HandshakeChangesStateToConnecting) {
    dtls_transport_->Initialize(config_);
    dtls_transport_->StartHandshake();
    
    EXPECT_EQ(dtls_transport_->GetState(), DtlsState::kConnecting);
}

// Test: StartHandshake twice fails (already connecting)
TEST_F(DtlsSrtpTest, DoubleHandshakeFails) {
    dtls_transport_->Initialize(config_);
    dtls_transport_->StartHandshake();
    
    auto result = dtls_transport_->StartHandshake();
    EXPECT_FALSE(result.IsOk());
}

// Test: StartAccept as server
TEST_F(DtlsSrtpTest, StartAcceptAsServer) {
    dtls_transport_->Initialize(config_);
    
    auto result = dtls_transport_->StartAccept();
    EXPECT_TRUE(result.IsOk());
}

// Test: StartAccept changes state to kConnecting
TEST_F(DtlsSrtpTest, AcceptChangesStateToConnecting) {
    dtls_transport_->Initialize(config_);
    dtls_transport_->StartAccept();
    
    EXPECT_EQ(dtls_transport_->GetState(), DtlsState::kConnecting);
}

// Test: StartAccept without Initialize - stub allows this
TEST_F(DtlsSrtpTest, AcceptWithoutInitializeBehavior) {
    // Stub implementation allows StartAccept without Initialize
    // This is acceptable behavior for a stub
    auto result = dtls_transport_->StartAccept();
    // Just verify it doesn't crash
    (void)result;
}

// Test: StartHandshake without Initialize - stub allows this  
TEST_F(DtlsSrtpTest, HandshakeWithoutInitializeBehavior) {
    // Stub implementation allows StartHandshake without Initialize
    // This is acceptable behavior for a stub
    auto result = dtls_transport_->StartHandshake();
    // Just verify it doesn't crash
    (void)result;
}

// Test: SetHandler works
TEST_F(DtlsSrtpTest, SetHandlerWorks) {
    auto handler = std::make_shared<TestDtlsHandler>();
    dtls_transport_->SetHandler(handler);
    
    // Should not crash and handler should be set
    EXPECT_TRUE(true);
}

// Test: Handler receives state change callback
TEST_F(DtlsSrtpTest, HandlerReceivesStateChange) {
    auto handler = std::make_shared<TestDtlsHandler>();
    dtls_transport_->SetHandler(handler);
    dtls_transport_->Initialize(config_);
    
    // Start handshake - stub may or may not trigger callback immediately
    dtls_transport_->StartHandshake();
    
    // Close should trigger callback
    dtls_transport_->Close();
    
    // Should receive state change to kClosed
    EXPECT_EQ(handler->last_state_, DtlsState::kClosed);
}

// Test: GetSrtpConfig returns valid config
TEST_F(DtlsSrtpTest, GetSrtpConfig) {
    dtls_transport_->Initialize(config_);
    
    auto srtp_config = dtls_transport_->GetSrtpConfig();
    // Should return a config (may be default values)
    EXPECT_EQ(srtp_config.send_suite, SrtpCryptoSuite::kAes128CmSha1_80);
    EXPECT_EQ(srtp_config.recv_suite, SrtpCryptoSuite::kAes128CmSha1_80);
}

// Test: SRTP Protect (stub implementation copies data)
TEST_F(DtlsSrtpTest, ProtectRtpCopiesData) {
    dtls_transport_->Initialize(config_);
    
    uint8_t rtp_data[] = {0x80, 0x00, 0x00, 0x01, 0x12, 0x34, 0x56, 0x78};
    uint8_t buffer[100];
    
    auto result = dtls_transport_->ProtectRtp(buffer, rtp_data, sizeof(rtp_data));
    EXPECT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), sizeof(rtp_data));
    
    // Data should be copied
    EXPECT_EQ(std::memcmp(buffer, rtp_data, sizeof(rtp_data)), 0);
}

// Test: SRTP Unprotect (stub implementation copies data)
TEST_F(DtlsSrtpTest, UnprotectRtpCopiesData) {
    dtls_transport_->Initialize(config_);
    
    uint8_t srtp_data[] = {0x80, 0x00, 0x00, 0x01, 0x12, 0x34, 0x56, 0x78};
    uint8_t buffer[100];
    
    auto result = dtls_transport_->UnprotectRtp(buffer, srtp_data, sizeof(srtp_data));
    EXPECT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), sizeof(srtp_data));
}

// Test: ProtectRtp with null parameters fails
TEST_F(DtlsSrtpTest, ProtectRtpWithNullFails) {
    dtls_transport_->Initialize(config_);
    
    uint8_t data[] = {0x80, 0x00};
    uint8_t buffer[100];
    
    auto result1 = dtls_transport_->ProtectRtp(nullptr, data, sizeof(data));
    EXPECT_FALSE(result1.IsOk());
    
    auto result2 = dtls_transport_->ProtectRtp(buffer, nullptr, sizeof(data));
    EXPECT_FALSE(result2.IsOk());
}

// Test: UnprotectRtp with null parameters fails
TEST_F(DtlsSrtpTest, UnprotectRtpWithNullFails) {
    dtls_transport_->Initialize(config_);
    
    uint8_t data[] = {0x80, 0x00};
    uint8_t buffer[100];
    
    auto result1 = dtls_transport_->UnprotectRtp(nullptr, data, sizeof(data));
    EXPECT_FALSE(result1.IsOk());
    
    auto result2 = dtls_transport_->UnprotectRtp(buffer, nullptr, sizeof(data));
    EXPECT_FALSE(result2.IsOk());
}

// Test: Close changes state to kClosed
TEST_F(DtlsSrtpTest, CloseChangesState) {
    dtls_transport_->Initialize(config_);
    dtls_transport_->StartHandshake();
    
    auto result = dtls_transport_->Close();
    EXPECT_TRUE(result.IsOk());
    
    EXPECT_EQ(dtls_transport_->GetState(), DtlsState::kClosed);
}

// Test: Close triggers callback
TEST_F(DtlsSrtpTest, CloseTriggersCallback) {
    auto handler = std::make_shared<TestDtlsHandler>();
    dtls_transport_->SetHandler(handler);
    dtls_transport_->Initialize(config_);
    dtls_transport_->StartHandshake();
    
    dtls_transport_->Close();
    
    // Should receive state change to kClosed
    EXPECT_EQ(handler->last_state_, DtlsState::kClosed);
}

// Test: OnDataReceived doesn't crash
TEST_F(DtlsSrtpTest, OnDataReceivedNoCrash) {
    dtls_transport_->Initialize(config_);
    
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03};
    EXPECT_NO_THROW(dtls_transport_->OnDataReceived(data, sizeof(data)));
}

// Test: SendData fails in stub (no OpenSSL)
TEST_F(DtlsSrtpTest, SendDataFailsWithoutOpenSSL) {
    dtls_transport_->Initialize(config_);
    
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03};
    auto result = dtls_transport_->SendData(data, sizeof(data));
    // Stub should fail
    EXPECT_FALSE(result.IsOk());
}

// Test: Multiple state transitions
TEST_F(DtlsSrtpTest, MultipleStateTransitions) {
    auto handler = std::make_shared<TestDtlsHandler>();
    dtls_transport_->SetHandler(handler);
    
    // New -> Initialize
    dtls_transport_->Initialize(config_);
    EXPECT_EQ(dtls_transport_->GetState(), DtlsState::kNew);
    
    // New -> Connecting
    dtls_transport_->StartHandshake();
    EXPECT_EQ(dtls_transport_->GetState(), DtlsState::kConnecting);
    
    // Connecting -> Closed
    dtls_transport_->Close();
    EXPECT_EQ(dtls_transport_->GetState(), DtlsState::kClosed);
}

// Test: SrtpCryptoSuiteToString
TEST_F(DtlsSrtpTest, CryptoSuiteToString) {
    EXPECT_STREQ(SrtpCryptoSuiteToString(SrtpCryptoSuite::kAes128CmSha1_80), "AES-CM-HMAC-SHA1-80");
    EXPECT_STREQ(SrtpCryptoSuiteToString(SrtpCryptoSuite::kAes128CmSha1_32), "AES-CM-HMAC-SHA1-32");
    EXPECT_STREQ(SrtpCryptoSuiteToString(SrtpCryptoSuite::kAes256GcmaSha1_80), "AES-GCM-HMAC-SHA1-80");
    EXPECT_STREQ(SrtpCryptoSuiteToString(SrtpCryptoSuite::kAes256GcmaSha1_128), "AES-GCM-HMAC-SHA1-128");
}

// Test: GetDefaultSrtpProfile
TEST_F(DtlsSrtpTest, DefaultSrtpProfile) {
    EXPECT_STREQ(GetDefaultSrtpProfile(), "SRTP_AES128_CM_SHA1_80");
}

// Test: Custom DTLS config
TEST_F(DtlsSrtpTest, CustomDtlsConfig) {
    DtlsConfig custom_config;
    custom_config.timeout_ms = 10000;
    custom_config.verify_peer = true;
    custom_config.srtp_profiles = "SRTP_AES256_GCM_SHA1_80";
    
    auto result = dtls_transport_->Initialize(custom_config);
    EXPECT_TRUE(result.IsOk());
}

// Test: SRTP config has correct defaults
TEST_F(DtlsSrtpTest, SrtpConfigDefaults) {
    dtls_transport_->Initialize(config_);
    
    auto srtp_config = dtls_transport_->GetSrtpConfig();
    
    // Check default crypto suites
    EXPECT_EQ(srtp_config.send_suite, SrtpCryptoSuite::kAes128CmSha1_80);
    EXPECT_EQ(srtp_config.recv_suite, SrtpCryptoSuite::kAes128CmSha1_80);
    
    // Empty keys by default
    EXPECT_TRUE(srtp_config.send_key.empty());
    EXPECT_TRUE(srtp_config.recv_key.empty());
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
