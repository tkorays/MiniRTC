/**
 * @file transport.h
 * @brief MiniRTC transport interface definitions
 */

#ifndef MINIRTC_TRANSPORT_H
#define MINIRTC_TRANSPORT_H

#include <memory>
#include <functional>

#include "transport_types.h"
#include "rtp_packet.h"

namespace minirtc {

// ============================================================================
// Transport Callback Interface
// ============================================================================

/// Transport callback interface
class ITransportCallback {
 public:
  /// Destructor
  virtual ~ITransportCallback() = default;

  /// RTP packet received callback
  virtual void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet,
                                   const NetworkAddress& from) = 0;

  /// RTCP packet received callback
  virtual void OnRtcpPacketReceived(const uint8_t* data,
                                    size_t size,
                                    const NetworkAddress& from) = 0;

  /// Transport error callback
  virtual void OnTransportError(TransportError error,
                                const std::string& message) = 0;

  /// Transport state changed callback
  virtual void OnTransportStateChanged(TransportState state) = 0;
};

// ============================================================================
// Transport Interface
// ============================================================================

/// Transport interface (abstract base class)
class ITransport {
 public:
  /// Destructor
  virtual ~ITransport() = default;

  /// Open transport
  /// @param config Transport configuration
  /// @return Transport error code
  virtual TransportError Open(const TransportConfig& config) = 0;

  /// Close transport
  virtual void Close() = 0;

  /// Get transport state
  virtual TransportState GetState() const = 0;

  /// Get transport type
  virtual TransportType GetType() const = 0;

  /// Send RTP packet
  /// @param packet RTP packet to send
  /// @return Transport error code
  virtual TransportError SendRtpPacket(std::shared_ptr<RtpPacket> packet) = 0;

  /// Receive RTP packet (blocking)
  /// @param packet Output packet
  /// @param from Source address
  /// @param timeout_ms Timeout in milliseconds
  /// @return Transport error code
  virtual TransportError ReceiveRtpPacket(std::shared_ptr<RtpPacket>* packet,
                                          NetworkAddress* from,
                                          int timeout_ms) = 0;

  /// Send RTCP packet
  /// @param data RTCP data
  /// @param size Data size
  /// @return Transport error code
  virtual TransportError SendRtcpPacket(const uint8_t* data, size_t size) = 0;

  /// Receive RTCP packet (blocking)
  /// @param buffer Output buffer
  /// @param buffer_size Buffer size
  /// @param received Received size
  /// @param from Source address
  /// @param timeout_ms Timeout in milliseconds
  /// @return Transport error code
  virtual TransportError ReceiveRtcpPacket(uint8_t* buffer,
                                            size_t buffer_size,
                                            size_t* received,
                                            NetworkAddress* from,
                                            int timeout_ms) = 0;

  /// Set callback
  virtual void SetCallback(std::shared_ptr<ITransportCallback> callback) = 0;

  /// Get local address
  virtual const NetworkAddress& GetLocalAddress() const = 0;

  /// Get remote address
  virtual const NetworkAddress& GetRemoteAddress() const = 0;

  /// Set remote address
  virtual TransportError SetRemoteAddress(const NetworkAddress& addr) = 0;

  /// Get transport statistics
  virtual TransportStats GetStats() const = 0;

  /// Reset statistics
  virtual void ResetStats() = 0;
};

// ============================================================================
// RTP Transport Callback Interface
// ============================================================================

/// RTP transport extended callback interface
class IRtpTransportCallback : public ITransportCallback {
 public:
  /// Destructor
  virtual ~IRtpTransportCallback() = default;

  /// RTX packet received callback (retransmission)
  virtual void OnRtxPacketReceived(std::shared_ptr<RtpPacket> packet,
                                   const NetworkAddress& from) = 0;

  /// FEC packet received callback
  virtual void OnFecPacketReceived(std::shared_ptr<RtpPacket> packet,
                                   const NetworkAddress& from) = 0;
};

// ============================================================================
// RTP Transport Interface
// ============================================================================

/// RTP transport interface
class IRTPTransport : public ITransport {
 public:
  /// Destructor
  virtual ~IRTPTransport() = default;

  /// Set RTP transport configuration
  /// @param config RTP transport configuration
  /// @return Transport error code
  virtual TransportError SetConfig(const RtpTransportConfig& config) = 0;

  /// Get RTP transport configuration
  virtual RtpTransportConfig GetConfig() const = 0;

  /// Send RTP data (raw data interface)
  /// @param data Data to send
  /// @param size Data size
  /// @param payload_type RTP payload type
  /// @param timestamp RTP timestamp
  /// @param marker Marker bit
  /// @return Transport error code
  virtual TransportError SendRtpData(const uint8_t* data,
                                      size_t size,
                                      uint8_t payload_type,
                                      uint32_t timestamp,
                                      bool marker) = 0;

  /// Send RTX packet
  /// @param original_seq Original sequence number
  /// @param data RTX data
  /// @param size Data size
  /// @return Transport error code
  virtual TransportError SendRtxPacket(uint16_t original_seq,
                                        const uint8_t* data,
                                        size_t size) = 0;

  /// Add remote candidate (for ICE)
  virtual void AddRemoteCandidate(const NetworkAddress& addr) = 0;

  /// Clear remote candidates
  virtual void ClearRemoteCandidates() = 0;

  /// Start receiving loop
  virtual void StartReceiving() = 0;

  /// Stop receiving loop
  virtual void StopReceiving() = 0;

  /// Get RTP receive statistics
  virtual RtpReceiveStats GetRtpReceiveStats() const = 0;

  /// Get RTP send statistics
  virtual RtpSendStats GetRtpSendStats() const = 0;

  /// Set loopback mode (for local testing without network)
  virtual void SetLoopbackMode(bool enabled) = 0;

  /// Check if loopback mode is enabled
  virtual bool IsLoopback() const = 0;
};

// ============================================================================
// RTCP Callback Interface
// ============================================================================

/// RTCP callback interface
class IRtcpCallback {
 public:
  /// Destructor
  virtual ~IRtcpCallback() = default;

  /// Sender Report received callback
  virtual void OnSenderReportReceived(uint32_t ssrc,
                                       uint64_t ntp_timestamp,
                                       uint32_t rtp_timestamp,
                                       uint32_t packet_count,
                                       uint32_t octet_count) = 0;

  /// Receiver Report received callback
  virtual void OnReceiverReportReceived(
      uint32_t ssrc,
      const std::vector<RtcpReportBlock>& blocks) = 0;

  /// NACK requested callback
  virtual void OnNackRequested(const std::vector<uint16_t>& seq_nums) = 0;

  /// BYE received callback
  virtual void OnByeReceived(uint32_t ssrc) = 0;

  /// RTCP stats updated callback
  virtual void OnRtcpStatsUpdated(const RtcpStats& stats) = 0;
};

// ============================================================================
// RTCP Module Interface
// ============================================================================

/// RTCP module interface
class IRTCPModule {
 public:
  /// Destructor
  virtual ~IRTCPModule() = default;

  /// Initialize
  /// @param local_ssrc Local SSRC
  /// @param remote_ssrc Remote SSRC
  /// @param config RTCP configuration
  virtual void Initialize(uint32_t local_ssrc,
                          uint32_t remote_ssrc,
                          const RtcpConfig& config) = 0;

  /// Set callback
  virtual void SetCallback(IRtcpCallback* callback) = 0;

  /// Bind RTP transport
  virtual void BindTransport(IRTPTransport* transport) = 0;

  /// Start RTCP module
  virtual void Start() = 0;

  /// Stop RTCP module
  virtual void Stop() = 0;

  /// Send Sender Report
  virtual void SendSr() = 0;

  /// Send Receiver Report
  virtual void SendRr() = 0;

  /// Send NACK (feedback)
  virtual void SendNack(const std::vector<uint16_t>& seq_nums) = 0;

  /// Send BYE
  virtual void SendBye(const std::string& reason = "") = 0;

  /// Process received RTCP packet
  virtual void OnRtcpPacketReceived(const uint8_t* data,
                                     size_t size,
                                     const NetworkAddress& from) = 0;

  /// Update sender statistics (called by RTPTransport)
  virtual void UpdateSenderStats(uint32_t packet_count,
                                  uint32_t octet_count,
                                  uint32_t timestamp) = 0;

  /// Update receiver statistics (called by RTPTransport)
  virtual void UpdateReceiverStats(uint16_t seq,
                                    uint32_t arrival_time_ms) = 0;

  /// Get RTCP statistics
  virtual RtcpStats GetStats() const = 0;

  /// Set configuration
  virtual void SetConfig(const RtcpConfig& config) = 0;

  /// Get configuration
  virtual RtcpConfig GetConfig() const = 0;
};

// ============================================================================
// SRTP Transport Interface (Reserved)
// ============================================================================

/// SRTP transport interface (reserved for future implementation)
class ISRTPTransport : public IRTPTransport {
 public:
  /// Destructor
  virtual ~ISRTPTransport() = default;

  /// Set SRTP policy
  /// @param policy SRTP policy
  /// @return Transport error code
  virtual TransportError SetSrtpPolicy(const SrtpPolicy& policy) = 0;

  /// Get SRTP policy
  virtual SrtpPolicy GetSrtpPolicy() const = 0;

  /// Update keys (for key rotation)
  /// @param send_key New send key
  /// @param recv_key New receive key
  /// @return Transport error code
  virtual TransportError UpdateKeys(const std::vector<uint8_t>& send_key,
                                     const std::vector<uint8_t>& recv_key) = 0;

  /// Get SRTP statistics
  virtual SrtpStats GetSrtpStats() const = 0;

  /// Set DTLS handler (reserved)
  virtual void SetDtlsHandler(void* handler) = 0;
};

// ============================================================================
// Network Interface (Reserved)
// ============================================================================

/// Network interface callback
class INetworkCallback {
 public:
  /// Destructor
  virtual ~INetworkCallback() = default;

  /// Data received callback
  virtual void OnDataReceived(const uint8_t* data,
                              size_t size,
                              const NetworkAddress& from) = 0;

  /// Error callback
  virtual void OnError(TransportError error, const std::string& message) = 0;
};

/// Network interface
class INetworkInterface {
 public:
  /// Destructor
  virtual ~INetworkInterface() = default;

  /// Create socket
  /// @param type Interface type
  /// @param local_addr Local address
  /// @return Transport error code
  virtual TransportError Create(NetworkInterfaceType type,
                                const NetworkAddress& local_addr) = 0;

  /// Close socket
  virtual void Close() = 0;

  /// Bind address
  virtual TransportError Bind(const NetworkAddress& addr) = 0;

  /// Connect (for TCP)
  virtual TransportError Connect(const NetworkAddress& addr) = 0;

  /// Listen (for TCP server)
  virtual TransportError Listen(int backlog) = 0;

  /// Accept connection (for TCP server)
  virtual TransportError Accept(std::shared_ptr<INetworkInterface>* client_socket,
                                NetworkAddress* client_addr) = 0;

  /// Send data
  virtual TransportError SendTo(const uint8_t* data,
                                size_t size,
                                const NetworkAddress& to) = 0;

  /// Receive data
  virtual TransportError ReceiveFrom(uint8_t* buffer,
                                      size_t buffer_size,
                                      size_t* received,
                                      NetworkAddress* from) = 0;

  /// Set socket options
  virtual TransportError SetOptions(const SocketOptions& options) = 0;

  /// Get local address
  virtual NetworkAddress GetLocalAddress() const = 0;

  /// Get remote address
  virtual NetworkAddress GetRemoteAddress() const = 0;

  /// Set callback
  virtual void SetCallback(INetworkCallback* callback) = 0;

  /// Set non-blocking mode
  virtual TransportError SetNonBlocking(bool enabled) = 0;

  /// Get socket file descriptor
  virtual int GetSocketFd() const = 0;

  /// Get interface type
  virtual NetworkInterfaceType GetType() const = 0;

  /// Check if socket is valid
  virtual bool IsValid() const = 0;
};

// ============================================================================
// Factory Functions
// ============================================================================

/// Create RTP transport
std::shared_ptr<IRTPTransport> CreateRTPTransport();

/// Create RTCP module
std::shared_ptr<IRTCPModule> CreateRTCPModule();

/// Create network interface
std::shared_ptr<INetworkInterface> CreateNetworkInterface();

/// Create SRTP transport (reserved)
std::shared_ptr<ISRTPTransport> CreateSRTPTransport();

}  // namespace minirtc

#endif  // MINIRTC_TRANSPORT_H
