/**
 * @file e2e_test.cc
 * @brief MiniRTC End-to-End test implementation
 */

#include "minirtc/e2e_test.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <random>

#include "minirtc/transport/rtp_packet.h"
#include "minirtc/transport/transport_types.h"

namespace minirtc {

// ============================================================================
// Constants
// ============================================================================

// Audio: OPUS payload type = 96
static constexpr uint8_t kAudioPayloadType = 96;
// Video: H264 payload type = 97
static constexpr uint8_t kVideoPayloadType = 97;

// Audio: 48kHz, 20ms frame
static constexpr uint32_t kAudioTimestampIncrement = 960;  // 48000 / 50

// Video: 30fps
static constexpr uint32_t kVideoTimestampIncrement = 3000;  // 90000 / 30

// SSRC values
static constexpr uint32_t kSsrcA = 0x12345678;
static constexpr uint32_t kSsrcB = 0x87654321;

// ============================================================================
// E2ETest Implementation
// ============================================================================

E2ETest::E2ETest() = default;

E2ETest::~E2ETest() {
    Stop();
}

bool E2ETest::Initialize(const E2EConfig& config) {
    config_ = config;
    
    // Create transport A (sender/receiver)
    transport_a_ = std::make_unique<RTPTransport>();
    
    // Configure transport A - use port 0 for random port
    RtpTransportConfig config_a;
    config_a.local_addr = NetworkAddress("0.0.0.0", 0);  // Random port
    config_a.remote_addr = NetworkAddress(config_.remote_ip, config_.remote_port_a);
    config_a.ssrc = kSsrcA;
    config_a.enable_rtcp = config_.enable_rtcp;
    
    auto error = transport_a_->SetConfig(config_a);
    if (error != TransportError::kOk) {
        std::cerr << "Failed to configure transport A: " << static_cast<int>(error) << std::endl;
        return false;
    }
    
    // Create transport B (sender/receiver)
    transport_b_ = std::make_unique<RTPTransport>();
    
    // Configure transport B - use port 0 for random port
    RtpTransportConfig config_b;
    config_b.local_addr = NetworkAddress("0.0.0.0", 0);  // Random port
    config_b.remote_addr = NetworkAddress(config_.remote_ip, config_.remote_port_b);
    config_b.ssrc = kSsrcB;
    config_b.enable_rtcp = config_.enable_rtcp;
    
    error = transport_b_->SetConfig(config_b);
    if (error != TransportError::kOk) {
        std::cerr << "Failed to configure transport B: " << static_cast<int>(error) << std::endl;
        return false;
    }
    
    // Initialize RTCP modules if enabled
    if (config_.enable_rtcp) {
        InitializeRtcp();
    }
    
    return true;
}

bool E2ETest::Start() {
    // Open transport A
    TransportConfig tconfig_a;
    tconfig_a.local_addr = NetworkAddress("0.0.0.0", 0);  // Random port
    tconfig_a.remote_addr = NetworkAddress(config_.remote_ip, config_.remote_port_a);
    tconfig_a.type = TransportType::kUdp;
    
    auto error = transport_a_->Open(tconfig_a);
    if (error != TransportError::kOk) {
        std::cerr << "Failed to open transport A: " << static_cast<int>(error) << std::endl;
        return false;
    }
    
    // Get actual port assigned by system
    auto local_addr_a = transport_a_->GetLocalAddress();
    uint16_t actual_port_a = local_addr_a.port;
    
    // Update remote address with actual port
    config_.remote_port_a = actual_port_a;
    config_.remote_ip = "127.0.0.1";
    
    // Open transport B
    TransportConfig tconfig_b;
    tconfig_b.local_addr = NetworkAddress("0.0.0.0", 0);  // Random port
    tconfig_b.remote_addr = NetworkAddress(config_.remote_ip, config_.remote_port_b);
    tconfig_b.type = TransportType::kUdp;
    
    error = transport_b_->Open(tconfig_b);
    if (error != TransportError::kOk) {
        std::cerr << "Failed to open transport B: " << static_cast<int>(error) << std::endl;
        transport_a_->Close();
        return false;
    }
    
    // Update remote address for B
    auto local_addr_b = transport_b_->GetLocalAddress();
    config_.remote_port_b = local_addr_b.port;
    
    // Start RTCP modules if enabled
    if (config_.enable_rtcp) {
        if (rtcp_module_a_) {
            rtcp_module_a_->Start();
        }
        if (rtcp_module_b_) {
            rtcp_module_b_->Start();
        }
    }
    
    running_ = true;
    return true;
}

void E2ETest::Stop() {
    running_ = false;
    
    // Stop RTCP modules
    if (rtcp_module_a_) {
        rtcp_module_a_->Stop();
    }
    if (rtcp_module_b_) {
        rtcp_module_b_->Stop();
    }
    
    if (transport_a_) {
        transport_a_->Close();
    }
    
    if (transport_b_) {
        transport_b_->Close();
    }
}

void E2ETest::RunSender(RTPTransport* transport, int duration_sec) {
    if (!transport || !running_) {
        return;
    }
    
    // Determine payload type based on SSRC
    auto config = transport->GetConfig();
    uint8_t payload_type = (config.ssrc == kSsrcA) ? kAudioPayloadType : kVideoPayloadType;
    uint32_t timestamp_increment = (config.ssrc == kSsrcA) ? kAudioTimestampIncrement : kVideoTimestampIncrement;
    
    // Generate dummy payload (simulate media data)
    std::vector<uint8_t> payload(160, 0);  // 160 bytes = 20ms audio @ 8kHz PCM
    
    uint32_t timestamp = 0;
    uint16_t seq = 0;
    uint32_t packet_count = 0;
    uint32_t octet_count = 0;
    
    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + std::chrono::seconds(duration_sec);
    
    // Random generator for payload variation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    // Get the corresponding RTCP module
    RTCPModule* rtcp_module = nullptr;
    if (config.ssrc == kSsrcA && rtcp_module_a_) {
        rtcp_module = rtcp_module_a_.get();
    } else if (config.ssrc == kSsrcB && rtcp_module_b_) {
        rtcp_module = rtcp_module_b_.get();
    }
    
    while (running_ && std::chrono::steady_clock::now() < end_time) {
        // Create RTP packet
        auto packet = std::make_shared<RtpPacket>(payload_type, timestamp, seq);
        packet->SetSsrc(config.ssrc);
        
        // Fill payload with random data
        for (size_t i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<uint8_t>(dis(gen));
        }
        packet->SetPayload(payload.data(), payload.size());
        
        // Serialize
        packet->Serialize();
        
        // Send
        transport->SendRtpPacket(packet);
        
        // Update RTCP sender stats
        packet_count++;
        octet_count += packet->GetSize();
        
        // Update RTCP module every packet
        if (rtcp_module) {
            rtcp_module->UpdateSenderStats(packet_count, octet_count, timestamp);
        }
        
        // Update
        timestamp += timestamp_increment;
        seq++;
        
        // Sleep for ~20ms (50 packets/sec for audio)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void E2ETest::RunReceiver(RTPTransport* transport, std::atomic<uint64_t>& received) {
    if (!transport) {
        return;
    }
    
    int timeout_count = 0;
    const int kMaxTimeouts = 30;
    
    while (running_) {
        NetworkAddress from;
        auto packet = std::make_shared<RtpPacket>();
        
        auto error = transport->ReceiveRtpPacket(&packet, &from, 100);
        
        if (error == TransportError::kOk && packet) {
            received++;
            timeout_count = 0;
        } else if (error == TransportError::kTimeout) {
            timeout_count++;
            if (timeout_count > kMaxTimeouts && !running_) {
                break;
            }
            continue;
        } else if (error == TransportError::kConnectionClosed || 
                   error == TransportError::kNotInitialized) {
            break;
        }
    }
}

TestResult E2ETest::TestAudioCall(int duration_sec) {
    TestResult result;
    
    // Only initialize if not already initialized
    if (!transport_a_ || !transport_b_) {
        if (!Initialize(config_)) {
            result.error_message = "Failed to initialize";
            return result;
        }
    }
    
    if (!Start()) {
        result.error_message = "Failed to start transport";
        return result;
    }
    
    std::atomic<uint64_t> received_a{0};
    std::atomic<uint64_t> received_b{0};
    
    // Start receiver threads
    std::thread receiver_a(&E2ETest::RunReceiver, this, transport_a_.get(), std::ref(received_a));
    std::thread receiver_b(&E2ETest::RunReceiver, this, transport_b_.get(), std::ref(received_b));
    
    // Give receivers time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Run sender on transport A
    RunSender(transport_a_.get(), duration_sec);
    
    // Wait for receivers to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Stop
    running_ = false;
    
    if (receiver_a.joinable()) receiver_a.join();
    if (receiver_b.joinable()) receiver_b.join();
    
    Stop();
    
    // Calculate results
    auto stats_a = transport_a_->GetStats();
    auto stats_b = transport_b_->GetStats();
    
    result.packets_sent = stats_a.packets_sent + stats_b.packets_sent;
    result.packets_received = received_a + received_b;
    
    if (result.packets_sent > 0) {
        result.loss_rate = 1.0 - (static_cast<double>(result.packets_received) / result.packets_sent);
    }
    
    // Success if loss rate < 10%
    result.success = result.loss_rate < 0.1;
    
    if (!result.success) {
        result.error_message = "Loss rate too high: " + std::to_string(result.loss_rate * 100) + "%";
    }
    
    return result;
}

TestResult E2ETest::TestVideoCall(int duration_sec) {
    TestResult result;
    
    // Only initialize if not already initialized
    if (!transport_a_ || !transport_b_) {
        if (!Initialize(config_)) {
            result.error_message = "Failed to initialize";
            return result;
        }
    }
    
    if (!Start()) {
        result.error_message = "Failed to start transport";
        return result;
    }
    
    std::atomic<uint64_t> received_a{0};
    std::atomic<uint64_t> received_b{0};
    
    // Start receiver threads
    std::thread receiver_a(&E2ETest::RunReceiver, this, transport_a_.get(), std::ref(received_a));
    std::thread receiver_b(&E2ETest::RunReceiver, this, transport_b_.get(), std::ref(received_b));
    
    // Give receivers time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Run sender on transport A with video parameters
    auto config = transport_a_->GetConfig();
    config.ssrc = kSsrcA;
    transport_a_->SetConfig(config);
    
    RunSender(transport_a_.get(), duration_sec);
    
    // Wait for receivers to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Stop
    running_ = false;
    
    if (receiver_a.joinable()) receiver_a.join();
    if (receiver_b.joinable()) receiver_b.join();
    
    Stop();
    
    // Calculate results
    auto stats_a = transport_a_->GetStats();
    auto stats_b = transport_b_->GetStats();
    
    result.packets_sent = stats_a.packets_sent + stats_b.packets_sent;
    result.packets_received = received_a + received_b;
    
    if (result.packets_sent > 0) {
        result.loss_rate = 1.0 - (static_cast<double>(result.packets_received) / result.packets_sent);
    }
    
    // Success if loss rate < 10%
    result.success = result.loss_rate < 0.1;
    
    if (!result.success) {
        result.error_message = "Loss rate too high: " + std::to_string(result.loss_rate * 100) + "%";
    }
    
    return result;
}

TestResult E2ETest::TestLoopback(int duration_sec) {
    TestResult result;
    
    // Only initialize if not already initialized
    if (!transport_a_ || !transport_b_) {
        if (!Initialize(config_)) {
            result.error_message = "Failed to initialize";
            return result;
        }
    }
    
    if (!Start()) {
        result.error_message = "Failed to start transport";
        return result;
    }
    
    std::atomic<uint64_t> received{0};
    
    // Start receiver thread on transport A
    std::thread receiver(&E2ETest::RunReceiver, this, transport_a_.get(), std::ref(received));
    
    // Give receiver time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Run sender on transport B
    RunSender(transport_b_.get(), duration_sec);
    
    // Wait for receiver to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Stop
    running_ = false;
    
    if (receiver.joinable()) receiver.join();
    
    Stop();
    
    // Print RTCP stats if enabled
    if (config_.enable_rtcp) {
        if (rtcp_module_a_) {
            auto stats_a = rtcp_module_a_->GetStats();
            std::cout << "[E2ETest] RTCP Module A - SR sent: " << stats_a.sr_sent 
                      << ", RR sent: " << stats_a.rr_sent << std::endl;
        }
        if (rtcp_module_b_) {
            auto stats_b = rtcp_module_b_->GetStats();
            std::cout << "[E2ETest] RTCP Module B - SR sent: " << stats_b.sr_sent 
                      << ", RR sent: " << stats_b.rr_sent << std::endl;
        }
    }
    
    // Calculate results
    auto stats_b = transport_b_->GetStats();
    auto recv_stats_a = transport_a_->GetRtpReceiveStats();
    
    result.packets_sent = stats_b.packets_sent;
    result.packets_received = received;
    
    if (result.packets_sent > 0) {
        result.loss_rate = 1.0 - (static_cast<double>(result.packets_received) / result.packets_sent);
    }
    
    // Success if loss rate < 10%
    result.success = result.loss_rate < 0.1;
    
    if (!result.success) {
        result.error_message = "Loss rate too high: " + std::to_string(result.loss_rate * 100) + "%";
    }
    
    return result;
}

void E2ETest::InitializeRtcp() {
    // Create RTCP module for transport A
    rtcp_module_a_ = std::make_unique<RTCPModule>();
    
    // Configure RTCP
    RtcpConfig rtcp_config;
    rtcp_config.enable = true;
    rtcp_config.enable_sr = true;
    rtcp_config.enable_rr = true;
    rtcp_config.enable_sdes = true;
    rtcp_config.interval_sr_ms = 5000;  // Send SR every 5 seconds
    rtcp_config.interval_rr_ms = 5000;
    rtcp_config.cname = "minirtc_a@local";
    rtcp_config.name = "MiniRTC A";
    
    // Initialize with local and remote SSRC
    rtcp_module_a_->Initialize(kSsrcA, kSsrcB, rtcp_config);
    
    // Bind to transport A
    rtcp_module_a_->BindTransport(transport_a_.get());
    
    // Create RTCP module for transport B
    rtcp_module_b_ = std::make_unique<RTCPModule>();
    
    // Configure RTCP for B
    rtcp_config.cname = "minirtc_b@local";
    rtcp_config.name = "MiniRTC B";
    
    rtcp_module_b_->Initialize(kSsrcB, kSsrcA, rtcp_config);
    
    // Bind to transport B
    rtcp_module_b_->BindTransport(transport_b_.get());
    
    std::cout << "[E2ETest] RTCP modules initialized" << std::endl;
}

}  // namespace minirtc
