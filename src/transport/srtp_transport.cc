/**
 * @file srtp_transport.cc
 * @brief MiniRTC SRTP transport (reserved for future implementation)
 */

#include "minirtc/transport/srtp_transport.h"

#include "minirtc/transport/rtp_transport.h"

namespace minirtc {

// ============================================================================
// SRTPTransport Implementation
// ============================================================================

SRTPTransport::SRTPTransport()
    : state_(TransportState::kClosed) {
}

SRTPTransport::~SRTPTransport() {
  Close();
}

TransportError SRTPTransport::Open(const TransportConfig& config) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (state_ != TransportState::kClosed) {
    return TransportError::kAlreadyExists;
  }

  // SRTP requires keys to be set first
  if (!keys_set_) {
    return TransportError::kNotInitialized;
  }

  // Create underlying RTP transport
  rtp_transport_ = CreateRTPTransport();
  if (!rtp_transport_) {
    return TransportError::kInternalError;
  }

  // Open with configuration
  TransportError error = rtp_transport_->Open(config);
  if (error != TransportError::kOk) {
    rtp_transport_.reset();
    return error;
  }

  state_ = TransportState::kOpen;
  return TransportError::kOk;
}

void SRTPTransport::Close() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (rtp_transport_) {
    rtp_transport_->Close();
    rtp_transport_.reset();
  }

  state_ = TransportState::kClosed;
}

TransportState SRTPTransport::GetState() const {
  return state_;
}

TransportType SRTPTransport::GetType() const {
  return TransportType::kSrtp;
}

TransportError SRTPTransport::SendRtpPacket(std::shared_ptr<RtpPacket> packet) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!rtp_transport_) {
    return TransportError::kNotInitialized;
  }

  // TODO: Encrypt packet before sending
  // For now, pass through to RTP transport
  TransportError error = rtp_transport_->SendRtpPacket(packet);

  if (error == TransportError::kOk) {
    stats_.send_encrypted_packets++;
  } else {
    stats_.send_encryption_failures++;
  }

  return error;
}

TransportError SRTPTransport::ReceiveRtpPacket(
    std::shared_ptr<RtpPacket>* packet,
    NetworkAddress* from,
    int timeout_ms) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!rtp_transport_) {
    return TransportError::kNotInitialized;
  }

  // Receive packet
  TransportError error = rtp_transport_->ReceiveRtpPacket(
      packet, from, timeout_ms);

  if (error != TransportError::kOk) {
    return error;
  }

  // TODO: Decrypt packet
  // For now, just count it as decrypted
  stats_.recv_decrypted_packets++;

  return TransportError::kOk;
}

TransportError SRTPTransport::SendRtcpPacket(const uint8_t* data, size_t size) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!rtp_transport_) {
    return TransportError::kNotInitialized;
  }

  // RTCP is typically not encrypted, but SRTCP can be
  // For now, pass through
  return rtp_transport_->SendRtcpPacket(data, size);
}

TransportError SRTPTransport::ReceiveRtcpPacket(
    uint8_t* buffer,
    size_t buffer_size,
    size_t* received,
    NetworkAddress* from,
    int timeout_ms) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!rtp_transport_) {
    return TransportError::kNotInitialized;
  }

  return rtp_transport_->ReceiveRtcpPacket(
      buffer, buffer_size, received, from, timeout_ms);
}

void SRTPTransport::SetCallback(ITransportCallback* callback) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (rtp_transport_) {
    rtp_transport_->SetCallback(callback);
  }
}

const NetworkAddress& SRTPTransport::GetLocalAddress() const {
  static NetworkAddress empty;
  if (rtp_transport_) {
    return rtp_transport_->GetLocalAddress();
  }
  return empty;
}

const NetworkAddress& SRTPTransport::GetRemoteAddress() const {
  static NetworkAddress empty;
  if (rtp_transport_) {
    return rtp_transport_->GetRemoteAddress();
  }
  return empty;
}

TransportError SRTPTransport::SetRemoteAddress(const NetworkAddress& addr) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!rtp_transport_) {
    return TransportError::kNotInitialized;
  }

  return rtp_transport_->SetRemoteAddress(addr);
}

TransportStats SRTPTransport::GetStats() const {
  std::lock_guard<std::mutex> lock(mutex_);

  TransportStats stats;
  if (rtp_transport_) {
    stats = rtp_transport_->GetStats();
  }
  return stats;
}

void SRTPTransport::ResetStats() {
  std::lock_guard<std::mutex> lock(mutex_);

  stats_ = SrtpStats();
  if (rtp_transport_) {
    rtp_transport_->ResetStats();
  }
}

TransportError SRTPTransport::SetConfig(const RtpTransportConfig& config) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!rtp_transport_) {
    return TransportError::kNotInitialized;
  }

  return rtp_transport_->SetConfig(config);
}

RtpTransportConfig SRTPTransport::GetConfig() const {
  std::lock_guard<std::mutex> lock(mutex_);

  if (rtp_transport_) {
    return rtp_transport_->GetConfig();
  }
  return RtpTransportConfig();
}

TransportError SRTPTransport::SendRtpData(const uint8_t* data,
                                          size_t size,
                                          uint8_t payload_type,
                                          uint32_t timestamp,
                                          bool marker) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!rtp_transport_) {
    return TransportError::kNotInitialized;
  }

  return rtp_transport_->SendRtpData(
      data, size, payload_type, timestamp, marker);
}

TransportError SRTPTransport::SendRtxPacket(uint16_t original_seq,
                                             const uint8_t* data,
                                             size_t size) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!rtp_transport_) {
    return TransportError::kNotInitialized;
  }

  return rtp_transport_->SendRtxPacket(original_seq, data, size);
}

void SRTPTransport::AddRemoteCandidate(const NetworkAddress& addr) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (rtp_transport_) {
    rtp_transport_->AddRemoteCandidate(addr);
  }
}

void SRTPTransport::ClearRemoteCandidates() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (rtp_transport_) {
    rtp_transport_->ClearRemoteCandidates();
  }
}

void SRTPTransport::StartReceiving() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (rtp_transport_) {
    rtp_transport_->StartReceiving();
  }
}

void SRTPTransport::StopReceiving() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (rtp_transport_) {
    rtp_transport_->StopReceiving();
  }
}

RtpReceiveStats SRTPTransport::GetRtpReceiveStats() const {
  std::lock_guard<std::mutex> lock(mutex_);

  if (rtp_transport_) {
    return rtp_transport_->GetRtpReceiveStats();
  }
  return RtpReceiveStats();
}

RtpSendStats SRTPTransport::GetRtpSendStats() const {
  std::lock_guard<std::mutex> lock(mutex_);

  if (rtp_transport_) {
    return rtp_transport_->GetRtpSendStats();
  }
  return RtpSendStats();
}

TransportError SRTPTransport::SetSrtpPolicy(const SrtpPolicy& policy) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Validate key sizes
  size_t key_size = 0;
  switch (policy.send_suite) {
    case SrtpCryptoSuite::kAesCm128HmacSha1_80:
    case SrtpCryptoSuite::kAesCm128HmacSha1_32:
      key_size = 30;  // 30 bytes: 30 for AES-CM
      break;
    case SrtpCryptoSuite::kAesGcm128:
      key_size = 16;  // 16 bytes key + 4 bytes salt
      break;
    case SrtpCryptoSuite::kAesGcm256:
      key_size = 32;  // 32 bytes key + 4 bytes salt
      break;
    default:
      return TransportError::kInvalidParam;
  }

  if (policy.send_key.size() < key_size ||
      policy.recv_key.size() < key_size) {
    return TransportError::kInvalidParam;
  }

  policy_ = policy;
  keys_set_ = true;

  return TransportError::kOk;
}

SrtpPolicy SRTPTransport::GetSrtpPolicy() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return policy_;
}

TransportError SRTPTransport::UpdateKeys(const std::vector<uint8_t>& send_key,
                                         const std::vector<uint8_t>& recv_key) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (send_key.empty() || recv_key.empty()) {
    return TransportError::kInvalidParam;
  }

  policy_.send_key = send_key;
  policy_.recv_key = recv_key;

  return TransportError::kOk;
}

SrtpStats SRTPTransport::GetSrtpStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return stats_;
}

void SRTPTransport::SetDtlsHandler(void* handler) {
  // Reserved for DTLS integration
  (void)handler;
}

// ============================================================================
// Factory
// ============================================================================

std::shared_ptr<ISRTPTransport> CreateSRTPTransport() {
  return std::make_shared<SRTPTransport>();
}

}  // namespace minirtc
