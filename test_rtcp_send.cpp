/**
 * @file test_rtcp_send.cc
 * @brief Simple test to verify RTCP SR sending
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include "minirtc/transport/rtp_transport.h"
#include "minirtc/transport/rtcp_module.h"
#include "minirtc/transport/transport_types.h"

using namespace minirtc;

int main() {
    std::cout << "=== RTCP SR Send Test ===" << std::endl;
    
    // Create RTP transport
    auto transport = CreateRTPTransport();
    
    // Configure transport
    RtpTransportConfig config;
    config.type = TransportType::kUdp;
    config.local_addr = NetworkAddress("127.0.0.1", 10000);
    config.remote_addr = NetworkAddress("127.0.0.1", 10001);
    config.ssrc = 0x12345678;
    config.loopback_mode = true;  // Use loopback mode
    
    auto error = transport->Open(config);
    if (error != TransportError::kOk) {
        std::cerr << "Failed to open transport: " << static_cast<int>(error) << std::endl;
        return 1;
    }
    
    std::cout << "Transport opened successfully" << std::endl;
    
    // Create RTCP module
    auto rtcp_module = CreateRTCPModule();
    
    // Configure RTCP
    RtcpConfig rtcp_config;
    rtcp_config.enable = true;
    rtcp_config.enable_sr = true;
    rtcp_config.enable_rr = false;
    rtcp_config.enable_sdes = true;
    rtcp_config.interval_sr_ms = 1000;  // Send SR every 1 second for testing
    rtcp_config.cname = "test@local";
    rtcp_config.name = "Test User";
    
    // Initialize RTCP module
    rtcp_module->Initialize(0x12345678, 0x87654321, rtcp_config);
    rtcp_module->BindTransport(transport.get());
    
    std::cout << "RTCP module initialized" << std::endl;
    
    // Start RTCP module
    rtcp_module->Start();
    
    std::cout << "RTCP module started, sending SR..." << std::endl;
    
    // Update sender stats (simulate sending RTP packets)
    for (int i = 0; i < 10; ++i) {
        rtcp_module->UpdateSenderStats(i + 1, (i + 1) * 160, i * 960);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    // Wait for RTCP timer to trigger (default is 5 seconds)
    std::cout << "Waiting for RTCP timer..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(6000));
    
    // Manual trigger to ensure SR is sent
    std::cout << "Manual SendSr() call..." << std::endl;
    rtcp_module->SendSr();
    
    // Get stats
    auto stats = rtcp_module->GetStats();
    std::cout << "\nRTCP Stats:" << std::endl;
    std::cout << "  SR sent: " << stats.sr_sent << std::endl;
    std::cout << "  RR sent: " << stats.rr_sent << std::endl;
    std::cout << "  SR received: " << stats.sr_received << std::endl;
    std::cout << "  RR received: " << stats.rr_received << std::endl;
    
    // Stop RTCP module
    rtcp_module->Stop();
    transport->Close();
    
    if (stats.sr_sent > 0) {
        std::cout << "\n✓ SUCCESS: RTCP SR packets were sent!" << std::endl;
        return 0;
    } else {
        std::cout << "\n✗ FAILED: No RTCP SR packets were sent" << std::endl;
        return 1;
    }
}
