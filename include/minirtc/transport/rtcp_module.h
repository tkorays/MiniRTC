/**
 * @file rtcp_module.h
 * @brief MiniRTC RTCP module implementation
 */

#ifndef MINIRTC_RTCP_MODULE_H
#define MINIRTC_RTCP_MODULE_H

#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>

#include "minirtc/transport/transport.h"
#include "minirtc/transport/transport_types.h"
#include "minirtc/transport/rtcp_packet.h"

namespace minirtc {

// ============================================================================
// RTCP Module Implementation
// ============================================================================

/// RTCP module implementation
class RTCPModule : public IRTCPModule {
 public:
  /// Constructor
  RTCPModule();

  /// Destructor
  ~RTCPModule() override;

  // ========================================================================
  // IRTCPModule Interface
  // ========================================================================

  /// Initialize
  void Initialize(uint32_t local_ssrc,
                  uint32_t remote_ssrc,
                  const RtcpConfig& config) override;

  /// Set callback
  void SetCallback(IRtcpCallback* callback) override;

  /// Bind RTP transport
  void BindTransport(IRTPTransport* transport) override;

  /// Start RTCP module
  void Start() override;

  /// Stop RTCP module
  void Stop() override;

  /// Send Sender Report
  void SendSr() override;

  /// Send Receiver Report
  void SendRr() override;

  /// Send NACK
  void SendNack(const std::vector<uint16_t>& seq_nums) override;

  /// Send BYE
  void SendBye(const std::string& reason = "") override;

  /// Process received RTCP packet
  void OnRtcpPacketReceived(const uint8_t* data,
                            size_t size,
                            const NetworkAddress& from) override;

  /// Update sender statistics
  void UpdateSenderStats(uint32_t packet_count,
                         uint32_t octet_count,
                         uint32_t timestamp) override;

  /// Update receiver statistics
  void UpdateReceiverStats(uint16_t seq,
                           uint32_t arrival_time_ms) override;

  /// Get RTCP statistics
  RtcpStats GetStats() const override;

  /// Set configuration
  void SetConfig(const RtcpConfig& config) override;

  /// Get configuration
  RtcpConfig GetConfig() const override;

 private:
  /// RTCP timer thread function
  void TimerLoop();

  /// Process received RTCP compound packet
  void ProcessCompoundPacket(const RtcpCompoundPacket& compound,
                             const NetworkAddress& from);

  /// Process Sender Report
  void ProcessSenderReport(const RtcpSrPacket& sr, const NetworkAddress& from);

  /// Process Receiver Report
  void ProcessReceiverReport(const RtcpRrPacket& rr, const NetworkAddress& from);

  /// Process NACK
  void ProcessNack(const RtcpNackPacket& nack);

  /// Process BYE
  void ProcessBye(const RtcpByePacket& bye);

  /// Build compound packet for SR
  std::shared_ptr<RtcpCompoundPacket> BuildSrCompound();

  /// Build compound packet for RR
  std::shared_ptr<RtcpCompoundPacket> BuildRrCompound();

  /// Get current NTP timestamp
  static void GetNtpTimestamp(uint32_t* high, uint32_t* low);

  /// Get current time in microseconds
  static uint64_t GetCurrentTimeUs();

  // Configuration
  RtcpConfig config_;
  uint32_t local_ssrc_ = 0;
  uint32_t remote_ssrc_ = 0;

  // State
  std::atomic<bool> running_{false};
  mutable std::mutex mutex_;

  // Callback
  IRtcpCallback* callback_ = nullptr;

  // Transport
  IRTPTransport* transport_ = nullptr;

  // Statistics
  RtcpStats stats_;

  // Sender info
  uint32_t sender_packet_count_ = 0;
  uint32_t sender_octet_count_ = 0;
  uint32_t last_sender_timestamp_ = 0;
  uint64_t last_sr_ntp_ = 0;
  uint32_t ntp_timestamp_high_ = 0;
  uint32_t ntp_timestamp_low_ = 0;

  // Receiver info
  uint16_t expected_seq_ = 0;
  uint16_t received_seq_ = 0;
  uint64_t total_received_ = 0;
  uint64_t total_expected_ = 0;
  uint32_t jitter_ = 0;
  uint32_t last_arrival_time_ms_ = 0;

  // Timers
  std::thread timer_thread_;
  std::condition_variable timer_cv_;
  std::atomic<bool> timer_stop_{false};

  // SDES info
  std::string cname_;
  std::string name_;
};

// ============================================================================
// Factory
// ============================================================================

/// Create RTCP module instance
std::shared_ptr<IRTCPModule> CreateRTCPModule();

}  // namespace minirtc

#endif  // MINIRTC_RTCP_MODULE_H
