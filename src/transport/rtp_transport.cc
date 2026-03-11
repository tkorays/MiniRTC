/**
 * @file rtp_transport.cc
 * @brief MiniRTC RTP transport implementation
 */

#include "minirtc/transport/rtp_transport.h"

#include <chrono>
#include <cstring>
#include <random>

#include "minirtc/transport/rtcp_module.h"

namespace minirtc {

// ============================================================================
// RTPTransport Implementation
// ============================================================================

RTPTransport::RTPTransport()
    : state_(TransportState::kClosed) {
  config_.type = TransportType::kUdp;
  packet_buffer_.reserve(RtpPacket::kMaxPacketSize);
}

RTPTransport::~RTPTransport() {
  Close();
}

TransportError RTPTransport::Open(const TransportConfig& config) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (state_ != TransportState::kClosed) {
    return TransportError::kAlreadyExists;
  }

  // Store config - copy base class fields
  // Note: RtpTransportConfig extended fields should be set via SetConfig() before Open()
  config_.type = config.type;
  config_.local_addr = config.local_addr;
  config_.remote_addr = config.remote_addr;
  config_.socket_buffer_size = config.socket_buffer_size;
  config_.enable_ipv6 = config.enable_ipv6;
  config_.timeout_ms = config.timeout_ms;

  // Check if loopback mode is requested (should be set via SetConfig before Open)
  bool is_loopback = config_.loopback_mode;

#ifdef _WIN32
  WSADATA wsa_data;
  WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

  // In loopback mode, we don't need actual sockets
  if (!is_loopback) {
    // Create RTP socket
    rtp_socket_ = CreateNetworkInterface();
    if (!rtp_socket_) {
      return TransportError::kSocketError;
    }

    TransportError error = rtp_socket_->Create(
        NetworkInterfaceType::kUdpSocket, config.local_addr);
    if (error != TransportError::kOk) {
      rtp_socket_.reset();
      return error;
    }

    // Create RTCP socket if enabled
    if (config_.enable_rtcp && config_.rtcp_port != 0) {
      NetworkAddress rtcp_addr = config.local_addr;
      rtcp_addr.port = config_.rtcp_port;

      rtcp_socket_ = CreateNetworkInterface();
      if (rtcp_socket_) {
        rtcp_socket_->Create(NetworkInterfaceType::kUdpSocket, rtcp_addr);
      }
    }
  } else {
    // Loopback mode: enable loopback flag
    loopback_mode_.store(true);
  }

  // Set random sequence number
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint16_t> dis(0, 65535);
  sequence_number_.store(dis(gen));

  state_ = TransportState::kOpen;
  return TransportError::kOk;
}

void RTPTransport::Close() {
  // IMPORTANT: Wake up waiting threads BEFORE stopping receiving
  // 1. First set loopback_mode_ to false and notify all
  loopback_mode_.store(false);
  loopback_cv_.notify_all();
  
  // 2. Stop receiving (this will also join the thread)
  StopReceiving();
  
  // 3. Now close sockets and clean up
  state_.store(TransportState::kClosed);
  
  std::lock_guard<std::mutex> lock(mutex_);

  if (rtcp_socket_) {
    rtcp_socket_->Close();
    rtcp_socket_.reset();
  }

  if (rtp_socket_) {
    rtp_socket_->Close();
    rtp_socket_.reset();
  }

  // Clear loopback queue
  loopback_queue_.clear();
}

TransportState RTPTransport::GetState() const {
  return state_.load();
}

TransportType RTPTransport::GetType() const {
  return config_.type;
}

TransportError RTPTransport::SendRtpPacket(std::shared_ptr<RtpPacket> packet) {
  if (state_.load() != TransportState::kOpen) {
    return TransportError::kNotInitialized;
  }

  if (!rtp_socket_ || !rtp_socket_->IsValid()) {
    // Allow loopback mode without valid socket (for testing)
    if (!loopback_mode_.load()) {
      return TransportError::kSocketError;
    }
  }

  // Set SSRC if not set (before serialization)
  if (packet->GetSsrc() == 0) {
    packet->SetSsrc(config_.ssrc);
  }

  // Set sequence number if not set (before serialization)
  if (packet->GetSequenceNumber() == 0) {
    packet->SetSequenceNumber(sequence_number_++);
  }

  // Serialize if needed (after setting SSRC/seq)
  // Always serialize to ensure buffer is correctly populated
  packet->Serialize();

  // Loopback mode: put packet in local queue for receiving
  if (loopback_mode_.load()) {
    NetworkAddress from;
    from.ip = "127.0.0.1";
    from.port = config_.local_addr.port;
    
    {
      std::lock_guard<std::mutex> lock(loopback_mutex_);
      loopback_queue_.push_back({packet, from});
    }
    loopback_cv_.notify_one();
    
    UpdateSendStats(packet->GetSize());
    return TransportError::kOk;
  }

  // Normal mode: send packet to remote
  NetworkAddress dest = config_.remote_addr;
  if (dest.ip.empty() || dest.port == 0) {
    if (remote_candidates_.empty()) {
      return TransportError::kInvalidParam;
    }
    dest = remote_candidates_[current_candidate_index_];
  }

  TransportError error = rtp_socket_->SendTo(
      packet->GetData(), packet->GetSize(), dest);

  if (error == TransportError::kOk) {
    UpdateSendStats(packet->GetSize());
  }

  return error;
}

TransportError RTPTransport::ReceiveRtpPacket(
    std::shared_ptr<RtpPacket>* packet,
    NetworkAddress* from,
    int timeout_ms) {
  if (state_.load() != TransportState::kOpen) {
    return TransportError::kNotInitialized;
  }

  // Loopback mode: receive from local queue
  if (loopback_mode_.load()) {
    std::unique_lock<std::mutex> lock(loopback_mutex_);
    
    // Wait for packet with timeout
    // Also check loopback_mode_ to allow immediate wakeup on close
    if (timeout_ms > 0) {
      auto wait_result = loopback_cv_.wait_for(
          lock, std::chrono::milliseconds(timeout_ms),
          [this] { return !loopback_queue_.empty() || !loopback_mode_.load(); });
      
      if (!wait_result) {
        return TransportError::kTimeout;
      }
      if (!loopback_mode_.load()) {
        return TransportError::kNotInitialized;
      }
    } else {
      // Non-blocking mode
      if (loopback_queue_.empty()) {
        return TransportError::kTimeout;
      }
    }
    
    // Get packet from queue - directly use the shared_ptr
    auto& item = loopback_queue_.front();
    *packet = item.first;  // Directly use the packet
    *from = item.second;
    loopback_queue_.pop_front();
    
    lock.unlock();

    // Update receive statistics
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    UpdateReceiveStats((*packet)->GetSequenceNumber(),
                       static_cast<uint64_t>(now) / 1000);

    return TransportError::kOk;
  }

  // Normal mode: receive from socket
  if (!rtp_socket_ || !rtp_socket_->IsValid()) {
    return TransportError::kSocketError;
  }

  packet_buffer_.resize(RtpPacket::kMaxPacketSize);
  size_t received = 0;

  TransportError error = rtp_socket_->ReceiveFrom(
      packet_buffer_.data(), packet_buffer_.size(), &received, from);

  if (error != TransportError::kOk) {
    return error;
  }

  // Parse RTP packet
  *packet = std::make_shared<RtpPacket>();
  if ((*packet)->Deserialize(packet_buffer_.data(), received) != 0) {
    return TransportError::kInvalidParam;
  }

  // Update receive statistics
  auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  UpdateReceiveStats((*packet)->GetSequenceNumber(),
                     static_cast<uint64_t>(now) / 1000);

  return TransportError::kOk;
}

TransportError RTPTransport::SendRtcpPacket(const uint8_t* data, size_t size) {
  if (state_.load() != TransportState::kOpen) {
    return TransportError::kNotInitialized;
  }

  if (!rtcp_socket_ || !rtcp_socket_->IsValid()) {
    return TransportError::kNotSupported;
  }

  NetworkAddress dest = config_.remote_addr;
  dest.port = config_.rtcp_port;

  return rtcp_socket_->SendTo(data, size, dest);
}

TransportError RTPTransport::ReceiveRtcpPacket(
    uint8_t* buffer,
    size_t buffer_size,
    size_t* received,
    NetworkAddress* from,
    int timeout_ms) {
  if (state_.load() != TransportState::kOpen) {
    return TransportError::kNotInitialized;
  }

  if (!rtcp_socket_ || !rtcp_socket_->IsValid()) {
    return TransportError::kNotSupported;
  }

  return rtcp_socket_->ReceiveFrom(buffer, buffer_size, received, from);
}

void RTPTransport::SetCallback(std::shared_ptr<ITransportCallback> callback) {
  // Store both the IRtpTransportCallback (if available) and the original ITransportCallback
  auto rtp_callback = std::dynamic_pointer_cast<IRtpTransportCallback>(callback);
  if (rtp_callback) {
    callback_ = rtp_callback;
  }
  // Always store the original callback for fallback
  transport_callback_ = callback;
}

const NetworkAddress& RTPTransport::GetLocalAddress() const {
  if (rtp_socket_) {
    return rtp_socket_->GetLocalAddress();
  }
  return config_.local_addr;
}

const NetworkAddress& RTPTransport::GetRemoteAddress() const {
  return config_.remote_addr;
}

TransportError RTPTransport::SetRemoteAddress(const NetworkAddress& addr) {
  std::lock_guard<std::mutex> lock(mutex_);
  config_.remote_addr = addr;
  return TransportError::kOk;
}

TransportStats RTPTransport::GetStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return stats_;
}

void RTPTransport::ResetStats() {
  std::lock_guard<std::mutex> lock(mutex_);
  stats_ = TransportStats();
  recv_stats_ = RtpReceiveStats();
  send_stats_ = RtpSendStats();
}

TransportError RTPTransport::SetConfig(const RtpTransportConfig& config) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  // Store old enable_nack state to detect changes
  bool old_enable_nack = config_.enable_nack;
  config_ = config;
  
  // Initialize NACK module if enabled
  if (config.enable_nack && !old_enable_nack) {
    enable_nack_ = true;
    nack_module_ = NackModuleFactory::Create();
    
    if (nack_module_) {
      NackConfig nack_cfg;
      nack_cfg.enable_nack = true;
      nack_cfg.enable_rtx = true;
      nack_cfg.mode = NackMode::kAdaptive;
      nack_cfg.max_retransmissions = 3;
      nack_cfg.rtt_estimate_ms = 100;
      nack_cfg.nack_timeout_ms = 200;
      nack_cfg.max_nack_list_size = 250;
      nack_cfg.nack_batch_interval_ms = 5;
      nack_cfg.nack_audio = true;
      nack_cfg.nack_video = true;
      
      // Set up callbacks for NACK and RTX
      nack_module_->SetOnNackRequestCallback([this](const std::vector<uint16_t>& seq_nums) {
        // Send NACK feedback via RTCP
        // This would typically be handled by the RTCP module
        fprintf(stderr, "[RTPTransport] NACK requested for %zu packets\n", seq_nums.size());
      });
      
      nack_module_->SetOnRtxPacketCallback([this](std::shared_ptr<RtpPacket> packet) {
        // Handle RTX packet (retransmitted packet)
        fprintf(stderr, "[RTPTransport] RTX packet received: seq=%u\n", 
                packet ? packet->GetSequenceNumber() : 0);
      });
      
      nack_module_->Initialize(nack_cfg);
      nack_module_->Start();
      fprintf(stderr, "[RTPTransport] NACK module initialized\n");
    }
  } else if (!config.enable_nack && old_enable_nack) {
    // Disable NACK
    enable_nack_ = false;
    if (nack_module_) {
      nack_module_->Stop();
      nack_module_->Reset();
      nack_module_.reset();
    }
  }
  
  return TransportError::kOk;
}

RtpTransportConfig RTPTransport::GetConfig() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return config_;
}

TransportError RTPTransport::SendRtpData(const uint8_t* data,
                                           size_t size,
                                           uint8_t payload_type,
                                           uint32_t timestamp,
                                           bool marker) {
  auto packet = std::make_shared<RtpPacket>(payload_type, timestamp,
                                             sequence_number_++);
  packet->SetPayload(data, size);
  packet->SetMarker(marker ? 1 : 0);
  packet->SetSsrc(config_.ssrc);

  return SendRtpPacket(packet);
}

TransportError RTPTransport::SendRtxPacket(uint16_t original_seq,
                                            const uint8_t* data,
                                            size_t size) {
  // RTX not implemented in base transport
  return TransportError::kNotSupported;
}

void RTPTransport::AddRemoteCandidate(const NetworkAddress& addr) {
  std::lock_guard<std::mutex> lock(mutex_);
  remote_candidates_.push_back(addr);
}

void RTPTransport::ClearRemoteCandidates() {
  std::lock_guard<std::mutex> lock(mutex_);
  remote_candidates_.clear();
  current_candidate_index_ = 0;
}

void RTPTransport::StartReceiving() {
  if (receiving_.exchange(true)) {
    return;  // Already receiving
  }

  receive_thread_ = std::thread([this]() {
    ReceiveLoop();
  });
}

void RTPTransport::StopReceiving() {
  receiving_.store(false);
  
  // Wake up any waiting threads
  loopback_cv_.notify_all();
  
  // Don't wait for thread - just detach it to avoid deadlock
  // The thread will exit when receiving_ is false
  if (receive_thread_.joinable()) {
    receive_thread_.detach();
  }
}

RtpReceiveStats RTPTransport::GetRtpReceiveStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return recv_stats_;
}

RtpSendStats RTPTransport::GetRtpSendStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return send_stats_;
}

void RTPTransport::SetLoopbackMode(bool enabled) {
  loopback_mode_.store(enabled);
  if (enabled) {
    // Clear any pending packets in the queue
    std::lock_guard<std::mutex> lock(loopback_mutex_);
    loopback_queue_.clear();
  }
}

bool RTPTransport::IsLoopback() const {
  return loopback_mode_.load();
}

void RTPTransport::ReceiveLoop() {
  fprintf(stderr, "[RTPTransport] ReceiveLoop: this=%p\n", this);
  while (receiving_.load()) {
    std::shared_ptr<RtpPacket> packet;
    NetworkAddress from;

    // Use non-blocking receive with timeout
    TransportError error = ReceiveRtpPacket(&packet, &from, 100);

    // Try IRtpTransportCallback first, fall back to ITransportCallback
    auto callback = callback_.lock();
    if (!callback) {
      callback = std::dynamic_pointer_cast<IRtpTransportCallback>(transport_callback_.lock());
    }
    
    if (error == TransportError::kOk && packet && callback) {
      // Handle NACK: inform NACK module about received packet
      if (nack_module_ && enable_nack_) {
        nack_module_->OnRtpPacketReceived(packet);
        
        // Get NACK list for missing packets and process them
        auto now = std::chrono::steady_clock::now().time_since_epoch().count() / 1000;  // ms
        auto nack_list = nack_module_->GetNackList(now);
        if (!nack_list.empty()) {
          fprintf(stderr, "[RTPTransport] NACK: %zu packets need retransmission\n", nack_list.size());
          // The NACK module callback will handle sending NACK requests
        }
      }
      
      // Dispatch to callback
      callback->OnRtpPacketReceived(packet, from);
    } else if (error != TransportError::kTimeout) {
      if (callback) {
        callback->OnTransportError(error, "Receive error");
      }
    }
  }
}

void RTPTransport::ProcessReceivedData(const uint8_t* data,
                                        size_t size,
                                        const NetworkAddress& from) {
  // Check if RTCP or RTP based on packet type
  if (size < 2) return;

  // Try IRtpTransportCallback first, fall back to ITransportCallback
  auto callback = callback_.lock();
  if (!callback) {
    callback = std::dynamic_pointer_cast<IRtpTransportCallback>(transport_callback_.lock());
  }
  if (!callback) return;

  // RTP/RTCP can be distinguished by payload type
  uint8_t pt = data[1] & 0x7F;

  if (pt >= 64 && pt <= 95) {
    // RTCP - could be handled separately
    callback->OnRtcpPacketReceived(data, size, from);
  } else {
    // RTP packet
    auto packet = std::make_shared<RtpPacket>();
    if (packet->Deserialize(data, size) == 0) {
      callback->OnRtpPacketReceived(packet, from);
    }
  }
}

void RTPTransport::UpdateSendStats(size_t packet_size) {
  std::lock_guard<std::mutex> lock(mutex_);

  stats_.packets_sent++;
  stats_.bytes_sent += packet_size;
  send_stats_.total_packets++;
  send_stats_.total_bytes += packet_size;
  send_stats_.next_seq = sequence_number_.load();

  auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  stats_.last_packet_timestamp_us = now / 1000;
}

void RTPTransport::UpdateReceiveStats(uint16_t seq, uint64_t arrival_time_us) {
  std::lock_guard<std::mutex> lock(mutex_);

  stats_.packets_received++;
  recv_stats_.total_packets++;
  recv_stats_.last_arrival_time_us = arrival_time_us;

  // Calculate sequence number difference
  if (recv_stats_.total_packets == 1) {
    recv_stats_.last_seq = seq;
    return;
  }

  uint16_t diff = seq - recv_stats_.last_seq;

  if (diff == 0) {
    // Duplicate packet
    return;
  }

  if (diff > 0x8000) {
    // Wrapped around
    if (seq > recv_stats_.last_seq) {
      // Wrapped forward
      recv_stats_.cycles++;
    }
    // Lost packets
    recv_stats_.packets_lost += diff;
    stats_.packets_lost += diff;
  }

  recv_stats_.last_seq = seq;
}

// ============================================================================
// Factory
// ============================================================================

std::shared_ptr<IRTPTransport> CreateRTPTransport() {
  return std::make_shared<RTPTransport>();
}

}  // namespace minirtc
