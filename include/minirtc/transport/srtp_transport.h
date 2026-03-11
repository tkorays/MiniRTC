/**
 * @file srtp_transport.h
 * @brief MiniRTC SRTP transport (reserved for future implementation)
 */

#ifndef MINIRTC_SRTP_TRANSPORT_H
#define MINIRTC_SRTP_TRANSPORT_H

#include <memory>
#include <vector>

#include "minirtc/transport/transport.h"
#include "minirtc/transport/transport_types.h"

namespace minirtc {

// ============================================================================
// SRTP Transport (Reserved)
// ============================================================================

/// SRTP transport (reserved for future DTLS integration)
class SRTPTransport : public ISRTPTransport {
 public:
  /// Constructor
  SRTPTransport();

  /// Destructor
  ~SRTPTransport() override;

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

  // ========================================================================
  // ISRTPTransport Interface
  // ========================================================================

  /// Set SRTP policy
  TransportError SetSrtpPolicy(const SrtpPolicy& policy) override;

  /// Get SRTP policy
  SrtpPolicy GetSrtpPolicy() const override;

  /// Update keys
  TransportError UpdateKeys(const std::vector<uint8_t>& send_key,
                            const std::vector<uint8_t>& recv_key) override;

  /// Get SRTP statistics
  SrtpStats GetSrtpStats() const override;

  /// Set DTLS handler (reserved)
  void SetDtlsHandler(void* handler) override;

 private:
  // SRTP not implemented yet - using RTP transport as placeholder
  std::shared_ptr<IRTPTransport> rtp_transport_;

  // SRTP state
  SrtpPolicy policy_;
  SrtpStats stats_;
  bool keys_set_ = false;

  // State
  mutable std::mutex mutex_;
  TransportState state_ = TransportState::kClosed;
};

// ============================================================================
// Factory
// ============================================================================

/// Create SRTP transport instance
std::shared_ptr<ISRTPTransport> CreateSRTPTransport();

}  // namespace minirtc

#endif  // MINIRTC_SRTP_TRANSPORT_H
