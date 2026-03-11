/**
 * @file e2e_test.h
 * @brief MiniRTC End-to-End test framework
 */

#ifndef MINIRTC_E2E_TEST_H_
#define MINIRTC_E2E_TEST_H_

#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include "minirtc/transport/rtp_transport.h"
#include "minirtc/stream_track.h"

namespace minirtc {

// End-to-end test configuration
struct E2EConfig {
    uint16_t local_port_a = 8000;
    uint16_t local_port_b = 8002;
    std::string remote_ip = "127.0.0.1";
    uint16_t remote_port_a = 8002;
    uint16_t remote_port_b = 8000;
    bool enable_rtcp = true;
};

// Test result
struct TestResult {
    bool success = false;
    uint64_t packets_sent = 0;
    uint64_t packets_received = 0;
    double loss_rate = 0.0;
    std::string error_message;
};

// End-to-end test class
class E2ETest {
public:
    E2ETest();
    ~E2ETest();
    
    // Initialize
    bool Initialize(const E2EConfig& config);
    
    // Start both ends
    bool Start();
    
    // Stop both ends
    void Stop();
    
    // Test 1-to-1 audio call
    TestResult TestAudioCall(int duration_sec = 5);
    
    // Test 1-to-1 video call
    TestResult TestVideoCall(int duration_sec = 5);
    
    // Test loopback
    TestResult TestLoopback(int duration_sec = 5);
    
private:
    void RunSender(RTPTransport* transport, int duration_sec);
    void RunReceiver(RTPTransport* transport, std::atomic<uint64_t>& received);
    
    E2EConfig config_;
    std::unique_ptr<RTPTransport> transport_a_;
    std::unique_ptr<RTPTransport> transport_b_;
    std::atomic<bool> running_{false};
};

}  // namespace minirtc

#endif  // MINIRTC_E2E_TEST_H_
