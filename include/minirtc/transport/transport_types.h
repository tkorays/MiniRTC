/**
 * @file transport_types.h
 * @brief MiniRTC transport layer type definitions
 */

#ifndef MINIRTC_TRANSPORT_TYPES_H
#define MINIRTC_TRANSPORT_TYPES_H

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace minirtc {

// ============================================================================
// Enumerations
// ============================================================================

/// Transport error codes
enum class TransportError {
  kOk = 0,                    ///< Success
  kNotInitialized,            ///< Not initialized
  kInvalidParam,              ///< Invalid parameter
  kSocketError,               ///< Socket error
  kTimeout,                   ///< Timeout
  kBufferOverflow,            ///< Buffer overflow
  kConnectionClosed,          ///< Connection closed
  kNotSupported,              ///< Unsupported operation
  kAlreadyExists,             ///< Already exists
  kNotFound,                  ///< Not found
  kInternalError,             ///< Internal error
};

/// Transport type
enum class TransportType {
  kUdp,                       ///< UDP transport
  kTcp,                       ///< TCP transport
  kSrtp,                      ///< SRTP transport
};

/// Transport state
enum class TransportState {
  kClosed,                    ///< Closed
  kOpening,                   ///< Opening
  kOpen,                      ///< Open
  kError,                     ///< Error
};

/// Network interface type
enum class NetworkInterfaceType {
  kUdpSocket,
  kTcpSocket,
  kLoopback,
};

/// RTCP packet types
enum class RtcpPacketType {
  kSR = 200,       ///< Sender Report
  kRR = 201,       ///< Receiver Report
  kSDES = 202,     ///< Source Description
  kBYE = 203,      ///< Goodbye
  kAPP = 204,      ///< Application Defined
  kRTPFB = 205,    ///< Transport Layer Feedback (NACK)
  kPSFB = 206,     ///< Payload-Specific Feedback (FEC)
};

#ifndef MINIRTC_ICE_CANDIDATE_TYPE_DEFINED
/// ICE candidate type
enum class IceCandidateType {
  kHost,       ///< Host candidate
  kSrflx,      ///< Server reflexive (STUN)
  kPrflx,      ///< Peer reflexive
  kRelayed,    ///< Relayed (TURN)
};
#define MINIRTC_ICE_CANDIDATE_TYPE_DEFINED
#endif

/// ICE candidate transport protocol
enum class IceCandidateTransport {
  kUdp,
  kTcp,
  kTls,
};

/// ICE component ID
enum class IceComponent : uint8_t {
  kRtp = 1,
  kRtcp = 2,
};

/// ICE role
enum class IceRole {
  kControlling,
  kControlled,
};

/// ICE state
enum class IceState {
  kNew,
  kChecking,
  kConnected,
  kCompleted,
  kFailed,
  kDisconnected,
  kClosed,
};

/// DTLS role
enum class DtlsRole {
  kClient,
  kServer,
};

/// DTLS state
enum class DtlsState {
  kNew,
  kConnecting,
  kConnected,
  kFailed,
  kClosed,
};

/// SRTP crypto suite
enum class SrtpCryptoSuite {
  kAesCm128HmacSha1_80,   ///< AES-CM with HMAC-SHA1 (80-bit tag)
  kAesCm128HmacSha1_32,   ///< AES-CM with HMAC-SHA1 (32-bit tag)
  kAesGcm128,             ///< AES-GCM (128-bit)
  kAesGcm256,             ///< AES-GCM (256-bit)
};

// ============================================================================
// Data Structures
// ============================================================================

/// Network address
struct NetworkAddress {
  std::string ip;             ///< IP address
  uint16_t port;             ///< Port number

  NetworkAddress() : ip("0.0.0.0"), port(0) {}
  NetworkAddress(const std::string& addr, uint16_t p) : ip(addr), port(p) {}

  bool IsValid() const { return !ip.empty() && port > 0; }

  bool operator==(const NetworkAddress& other) const {
    return ip == other.ip && port == other.port;
  }

  bool operator!=(const NetworkAddress& other) const {
    return !(*this == other);
  }

  std::string ToString() const {
    return ip + ":" + std::to_string(port);
  }
};

#ifndef MINIRTC_TRANSPORT_STATS_DEFINED
/// Transport statistics
struct TransportStats {
  uint64_t packets_sent = 0;            ///< Packets sent
  uint64_t packets_received = 0;       ///< Packets received
  uint64_t bytes_sent = 0;             ///< Bytes sent
  uint64_t bytes_received = 0;         ///< Bytes received
  uint64_t packets_lost = 0;           ///< Packets lost
  uint32_t round_trip_time_ms = 0;     ///< RTT in milliseconds
  float jitter_ms = 0.0f;              ///< Jitter in milliseconds
  uint32_t sender_bitrate_bps = 0;    ///< Sender bitrate (bps)
  uint32_t receiver_bitrate_bps = 0;   ///< Receiver bitrate (bps)
  uint64_t last_packet_timestamp_us = 0; ///< Last packet timestamp (microseconds)
};
#define MINIRTC_TRANSPORT_STATS_DEFINED
#endif

/// Transport configuration
struct TransportConfig {
  TransportType type = TransportType::kUdp;
  NetworkAddress local_addr;           ///< Local address
  NetworkAddress remote_addr;          ///< Remote address
  uint32_t socket_buffer_size = 65536; ///< Socket buffer size
  bool enable_ipv6 = false;            ///< Enable IPv6
  int timeout_ms = 3000;              ///< Timeout in milliseconds
  bool loopback_mode = false;          ///< Enable loopback mode for local testing
};

/// Socket options
struct SocketOptions {
  bool reuse_addr = true;       ///< Address reuse
  bool reuse_port = false;      ///< Port reuse
  bool non_blocking = false;    ///< Non-blocking mode
  bool broadcast = false;       ///< Broadcast
  int send_buffer_size = 0;     ///< Send buffer size (0 = system default)
  int recv_buffer_size = 0;    ///< Receive buffer size (0 = system default)
  int ttl = 128;               ///< TTL
  int tos = 0;                 ///< Type of Service
};

/// RTCP report block
struct RtcpReportBlock {
  uint32_t ssrc;               ///< Synchronization source SSRC
  uint8_t fraction_lost;       ///< Fraction lost (8 bits)
  int32_t packets_lost;        ///< Cumulative packets lost (24 bits)
  uint32_t highest_seq;        ///< Highest sequence number received
  uint32_t jitter;             ///< Interarrival jitter
  uint32_t lsr;                ///< Last SR timestamp
  uint32_t dlsr;               ///< Delay since last SR
};

/// RTCP configuration
struct RtcpConfig {
  bool enable = true;                        ///< Enable RTCP
  bool enable_sr = true;                    ///< Send SR
  bool enable_rr = true;                    ///< Send RR
  bool enable_sdes = true;                  ///< Send SDES
  bool enable_nack = false;                  ///< Enable NACK feedback
  bool enable_fb = false;                    ///< Enable feedback messages

  uint32_t interval_sr_ms = 5000;          ///< SR interval (milliseconds)
  uint32_t interval_rr_ms = 5000;          ///< RR interval (milliseconds)
  uint32_t interval_sdes_ms = 10000;       ///< SDES interval (milliseconds)

  std::string cname;                       ///< CNAME for SDES
  std::string name;                        ///< Username for SDES

  size_t max_report_blocks = 31;           ///< Max report blocks
};

/// RTCP statistics
struct RtcpStats {
  uint64_t sr_sent = 0;                    ///< SR sent
  uint64_t rr_sent = 0;                    ///< RR sent
  uint64_t sr_received = 0;                ///< SR received
  uint64_t rr_received = 0;                 ///< RR received
  uint64_t nack_sent = 0;                  ///< NACK sent
  uint64_t nack_received = 0;               ///< NACK received

  uint64_t last_sr_timestamp = 0;          ///< Last SR NTP timestamp
  uint64_t last_sr_time_us = 0;            ///< Last SR local time
  uint32_t avg_rtt_ms = 0;                 ///< Average RTT (milliseconds)

  std::vector<RtcpReportBlock> last_report_blocks;  ///< Last report blocks
};

/// RTP receive statistics
struct RtpReceiveStats {
  uint64_t total_packets = 0;              ///< Total packets
  uint64_t total_bytes = 0;               ///< Total bytes
  uint16_t last_seq = 0;                  ///< Last sequence number
  uint16_t cycles = 0;                    ///< Sequence number cycles
  uint64_t packets_lost = 0;              ///< Packets lost
  float fraction_lost = 0.0f;             ///< Fraction lost
  uint32_t jitter = 0;                    ///< Jitter (RTP timestamp units)
  uint64_t last_arrival_time_us = 0;      ///< Last arrival time
};

/// RTP send statistics
struct RtpSendStats {
  uint64_t total_packets = 0;              ///< Total packets
  uint64_t total_bytes = 0;               ///< Total bytes
  uint16_t next_seq = 0;                  ///< Next sequence number
  uint32_t timestamp = 0;                 ///< Current timestamp
  uint32_t bitrate_bps = 0;               ///< Current bitrate
};

/// RTP transport configuration
struct RtpTransportConfig : public TransportConfig {
  uint32_t ssrc = 0;                      ///< SSRC identifier
  uint16_t rtcp_port = 0;                ///< RTCP port (even + 1)
  bool enable_rtcp = true;               ///< Enable RTCP
  bool enable_nack = false;               ///< Enable NACK
  bool enable_fec = false;                ///< Enable FEC
  int max_packet_size = 1500;            ///< Max packet size
  bool enable_rtx = false;                ///< Enable retransmission
  uint8_t rtx_payload_type = 0;           ///< RTX payload type
  bool loopback_mode = false;            ///< Enable loopback mode
};

#ifndef MINIRTC_ICE_CANDIDATE_DEFINED
/// ICE candidate
struct IceCandidate {
  uint32_t foundation;                    ///< Foundation identifier
  IceComponent component;                 ///< Component ID
  IceCandidateTransport transport;        ///< Transport protocol
  IceCandidateType type;                  ///< Candidate type
  uint32_t priority;                      ///< Priority
  NetworkAddress address;                ///< Address
  std::string username;                  ///< Username (for TURN)
  std::string password;                  ///< Password (for TURN)

  std::string ToString() const;           ///< Convert to SDP format
};
#define MINIRTC_ICE_CANDIDATE_DEFINED
#endif

/// ICE configuration
struct IceConfig {
  std::vector<std::string> stun_servers;     ///< STUN servers
  std::vector<std::string> turn_servers;    ///< TURN servers
  std::string username;                     ///< TURN username
  std::string password;                     ///< TURN password

  int ice_timeout_ms = 25000;               ///< ICE timeout
  int candidate_timeout_ms = 2000;          ///< Candidate timeout

  bool ice_lite = false;                    ///< ICE-Lite mode
};

/// DTLS fingerprint (for SDP)
struct DtlsFingerprint {
  std::string algorithm;                   ///< Algorithm (e.g., "SHA-256")
  std::string fingerprint;                 ///< Certificate fingerprint
};

/// DTLS configuration
struct DtlsConfig {
  DtlsRole role = DtlsRole::kClient;

  std::string certificate;                 ///< Certificate (PEM)
  std::string private_key;                 ///< Private key (PEM)

  bool srtp_profiles = true;               ///< SRTP profiles

  bool enable_srtp = true;
  bool enable_renegotiation = false;
};

/// SRTP policy
struct SrtpPolicy {
  SrtpCryptoSuite send_suite = SrtpCryptoSuite::kAesCm128HmacSha1_80;
  SrtpCryptoSuite recv_suite = SrtpCryptoSuite::kAesCm128HmacSha1_80;

  std::vector<uint8_t> send_key;          ///< Send key
  std::vector<uint8_t> recv_key;          ///< Receive key

  uint32_t replay_window_size = 128;      ///< Replay window size
};

/// SRTP statistics
struct SrtpStats {
  uint64_t send_encrypted_packets = 0;    ///< Encrypted packets sent
  uint64_t recv_decrypted_packets = 0;    ///< Decrypted packets received
  uint64_t send_encryption_failures = 0;  ///< Encryption failures
  uint64_t recv_decryption_failures = 0;  ///< Decryption failures
  uint64_t recv_auth_failures = 0;        ///< Authentication failures
  uint64_t recv_replay_failures = 0;      ///< Replay failures
};

/// Network interface information
struct NetworkInterfaceInfo {
  std::string name;                        ///< Interface name
  std::string display_name;               ///< Display name
  std::vector<NetworkAddress> addresses;  ///< IP addresses
  bool is_loopback = false;               ///< Is loopback
  bool is_up = false;                     ///< Is up
  bool is_ipv6 = false;                   ///< Supports IPv6
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Convert TransportError to string
inline const char* TransportErrorToString(TransportError error) {
  switch (error) {
    case TransportError::kOk: return "OK";
    case TransportError::kNotInitialized: return "Not initialized";
    case TransportError::kInvalidParam: return "Invalid parameter";
    case TransportError::kSocketError: return "Socket error";
    case TransportError::kTimeout: return "Timeout";
    case TransportError::kBufferOverflow: return "Buffer overflow";
    case TransportError::kConnectionClosed: return "Connection closed";
    case TransportError::kNotSupported: return "Not supported";
    case TransportError::kAlreadyExists: return "Already exists";
    case TransportError::kNotFound: return "Not found";
    case TransportError::kInternalError: return "Internal error";
    default: return "Unknown";
  }
}

}  // namespace minirtc

#endif  // MINIRTC_TRANSPORT_TYPES_H
