/**
 * @file rtp_transport.h
 * @brief MiniRTC RTP transport implementation
 */

#ifndef MINIRTC_RTP_TRANSPORT_H
#define MINIRTC_RTP_TRANSPORT_H

#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <deque>

#include "minirtc/transport/transport.h"
#include "minirtc/transport/transport_types.h"

namespace minirtc {

// ============================================================================
// RTP Transport Implementation
// ============================================================================

/// RTP transport implementation
class RTPTransport : public IRTPTransport {
 public:
  /// Constructor
  RTPTransport();

  /// Destructor
  ~RTPTransport() override;

  // ========================================================================
  // ITransport Interface
  // ========================================================================

  /// Open transport
  TransportError Open(const TransportConfig& config) override;

  /// Close transport
  void Close() override;

  /// Get transport state
  TransportState GetState() const override;

  /// Get transport type
  TransportType GetType() const override;

  /// Send RTP packet
  TransportError SendRtpPacket(std::shared_ptr<RtpPacket> packet) override;

  /// Receive RTP packet
  TransportError ReceiveRtpPacket(std::shared_ptr<RtpPacket>* packet,
                                  NetworkAddress* from,
                                  int timeout_ms) override;

  /// Send RTCP packet
  TransportError SendRtcpPacket(const uint8_t* data, size_t size) override;

  /// Receive RTCP packet
  TransportError ReceiveRtcpPacket(uint8_t* buffer,
                                    size_t buffer_size,
                                    size_t* received,
                                    NetworkAddress* from,
                                    int timeout_ms) override;

  /// Set callback
  void SetCallback(std::shared_ptr<ITransportCallback> callback) override;

  /// Get local address
  const NetworkAddress& GetLocalAddress() const override;

  /// Get remote address
  const NetworkAddress& GetRemoteAddress() const override;

  /// Set remote address
  TransportError SetRemoteAddress(const NetworkAddress& addr) override;

  /// Get transport statistics
  TransportStats GetStats() const override;

  /// Reset statistics
  void ResetStats() override;

  // ========================================================================
  // IRTPTransport Interface
  // ========================================================================

  /// Set RTP transport configuration
  TransportError SetConfig(const RtpTransportConfig& config) override;

  /// Get RTP transport configuration
  RtpTransportConfig GetConfig() const override;

  /// Send RTP data
  TransportError SendRtpData(const uint8_t* data,
                              size_t size,
                              uint8_t payload_type,
                              uint32_t timestamp,
                              bool marker) override;

  /// Send RTX packet
  TransportError SendRtxPacket(uint16_t original_seq,
                                const uint8_t* data,
                                size_t size) override;

  /// Add remote candidate
  void AddRemoteCandidate(const NetworkAddress& addr) override;

  /// Clear remote candidates
  void ClearRemoteCandidates() override;

  /// Start receiving
  void StartReceiving() override;

  /// Stop receiving
  void StopReceiving() override;

  /// Get RTP receive statistics
  RtpReceiveStats GetRtpReceiveStats() const override;

  /// Get RTP send statistics
  RtpSendStats GetRtpSendStats() const override;

  /// Set loopback mode (for local testing without network)
  void SetLoopbackMode(bool enabled) override;

  /// Check if loopback mode is enabled
  bool IsLoopback() const override;

 private:
  /// Receive thread function
  void ReceiveLoop();

  /// Process received data
  void ProcessReceivedData(const uint8_t* data, size_t size,
                           const NetworkAddress& from);

  /// Update send statistics
  void UpdateSendStats(size_t packet_size);

  /// Update receive statistics
  void UpdateReceiveStats(uint16_t seq, uint64_t arrival_time_us);

  // Configuration
  RtpTransportConfig config_;

  // State
  std::atomic<TransportState> state_;
  mutable std::mutex mutex_;

  // Callbacks
  std::weak_ptr<IRtpTransportCallback> callback_;

  // Network
  std::shared_ptr<INetworkInterface> rtp_socket_;
  std::shared_ptr<INetworkInterface> rtcp_socket_;

  // Remote candidates
  std::vector<NetworkAddress> remote_candidates_;
  size_t current_candidate_index_ = 0;

  // Sequence number
  std::atomic<uint16_t> sequence_number_{0};

  // Statistics
  TransportStats stats_;
  RtpReceiveStats recv_stats_;
  RtpSendStats send_stats_;

  // Receive thread
  std::thread receive_thread_;
  std::atomic<bool> receiving_{false};

  // Packet buffer
  std::vector<uint8_t> packet_buffer_;

  // NACK state
  std::mutex nack_mutex_;
  std::vector<uint16_t> pending_nacks_;

  // Loopback mode support
  std::atomic<bool> loopback_mode_{false};
  std::deque<std::pair<std::vector<uint8_t>, NetworkAddress>> loopback_queue_;
  std::mutex loopback_mutex_;
  std::condition_variable loopback_cv_;
};

// ============================================================================
// Factory
// ============================================================================

/// Create RTP transport instance
std::shared_ptr<IRTPTransport> CreateRTPTransport();

}  // namespace minirtc

#endif  // MINIRTC_RTP_TRANSPORT_H
