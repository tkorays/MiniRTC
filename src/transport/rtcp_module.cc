/**
 * @file rtcp_module.cc
 * @brief MiniRTC RTCP module implementation
 */

#include "minirtc/transport/rtcp_module.h"

#include <chrono>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <random>

#include "minirtc/transport/rtp_transport.h"

namespace minirtc {

// ============================================================================
// RTCPModule Implementation
// ============================================================================

RTCPModule::RTCPModule()
    : running_(false) {
}

RTCPModule::~RTCPModule() {
  Stop();
}

void RTCPModule::Initialize(uint32_t local_ssrc,
                            uint32_t remote_ssrc,
                            const RtcpConfig& config) {
  std::lock_guard<std::mutex> lock(mutex_);

  local_ssrc_ = local_ssrc;
  remote_ssrc_ = remote_ssrc;
  config_ = config;

  // Set SDES info
  cname_ = config.cname.empty() ? "minirtc@local" : config.cname;
  name_ = config.name.empty() ? "MiniRTC User" : config.name;

  // Initialize sequence number
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint16_t> dis(0, 65535);
  expected_seq_ = dis(gen);
}

void RTCPModule::SetCallback(IRtcpCallback* callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  callback_ = callback;
}

void RTCPModule::BindTransport(IRTPTransport* transport) {
  std::lock_guard<std::mutex> lock(mutex_);
  transport_ = transport;
}

void RTCPModule::Start() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (running_.exchange(true)) {
    return;  // Already running
  }

  timer_stop_.store(false);

  timer_thread_ = std::thread([this]() {
    TimerLoop();
  });
}

void RTCPModule::Stop() {
  timer_stop_.store(true);
  timer_cv_.notify_all();

  if (timer_thread_.joinable()) {
    timer_thread_.join();
  }

  running_.store(false);
}

void RTCPModule::SendSr() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!transport_ || !config_.enable_sr) {
    return;
  }

  auto compound = BuildSrCompound();
  if (!compound) {
    return;
  }

  std::vector<uint8_t> buffer(compound->GetSize());
  compound->Serialize(buffer.data(), buffer.size());

  TransportError error = transport_->SendRtcpPacket(
      buffer.data(), buffer.size());

  if (error == TransportError::kOk) {
    stats_.sr_sent++;
    stats_.last_sr_timestamp = GetCurrentTimeUs();
  }
}

void RTCPModule::SendRr() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!transport_ || !config_.enable_rr) {
    return;
  }

  auto compound = BuildRrCompound();
  if (!compound) {
    return;
  }

  std::vector<uint8_t> buffer(compound->GetSize());
  compound->Serialize(buffer.data(), buffer.size());

  TransportError error = transport_->SendRtcpPacket(
      buffer.data(), buffer.size());

  if (error == TransportError::kOk) {
    stats_.rr_sent++;
  }
}

void RTCPModule::SendNack(const std::vector<uint16_t>& seq_nums) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!transport_ || !config_.enable_nack) {
    return;
  }

  auto nack = CreateRtcpNack(local_ssrc_, remote_ssrc_, seq_nums);
  auto compound = CreateRtcpCompound();
  compound->AddPacket(nack);

  std::vector<uint8_t> buffer(compound->GetSize());
  compound->Serialize(buffer.data(), buffer.size());

  TransportError error = transport_->SendRtcpPacket(
      buffer.data(), buffer.size());

  if (error == TransportError::kOk) {
    stats_.nack_sent++;
  }
}

void RTCPModule::SendBye(const std::string& reason) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!transport_) {
    return;
  }

  auto bye = CreateRtcpBye({local_ssrc_}, reason);
  auto compound = CreateRtcpCompound();
  compound->AddPacket(bye);

  std::vector<uint8_t> buffer(compound->GetSize());
  compound->Serialize(buffer.data(), buffer.size());

  transport_->SendRtcpPacket(buffer.data(), buffer.size());
}

void RTCPModule::OnRtcpPacketReceived(const uint8_t* data,
                                       size_t size,
                                       const NetworkAddress& from) {
  std::lock_guard<std::mutex> lock(mutex_);

  RtcpCompoundPacket compound;
  if (compound.Deserialize(data, size) != 0) {
    return;
  }

  ProcessCompoundPacket(compound, from);
}

void RTCPModule::UpdateSenderStats(uint32_t packet_count,
                                    uint32_t octet_count,
                                    uint32_t timestamp) {
  std::lock_guard<std::mutex> lock(mutex_);

  sender_packet_count_ = packet_count;
  sender_octet_count_ = octet_count;
  last_sender_timestamp_ = timestamp;

  // Get NTP timestamp
  GetNtpTimestamp(&ntp_timestamp_high_, &ntp_timestamp_low_);
}

void RTCPModule::UpdateReceiverStats(uint16_t seq,
                                       uint32_t arrival_time_ms) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Update expected and received counts
  int16_t diff = static_cast<int16_t>(seq - expected_seq_);

  if (diff > 0) {
    // Packets received out of order or lost
    total_expected_ += diff;
  }

  total_received_++;
  received_seq_ = seq;

  // Simple jitter calculation
  if (last_arrival_time_ms_ != 0) {
    int32_t transit = static_cast<int32_t>(arrival_time_ms) -
                      static_cast<int32_t>(last_arrival_time_ms_);
    int32_t jitter_diff = transit - jitter_;
    jitter_ += (jitter_diff + 8) / 16;  // Smoothed jitter
  }

  last_arrival_time_ms_ = arrival_time_ms;
  expected_seq_ = seq + 1;
}

RtcpStats RTCPModule::GetStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return stats_;
}

void RTCPModule::SetConfig(const RtcpConfig& config) {
  std::lock_guard<std::mutex> lock(mutex_);
  config_ = config;
}

RtcpConfig RTCPModule::GetConfig() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return config_;
}

void RTCPModule::TimerLoop() {
  auto next_sr = std::chrono::steady_clock::now() +
                 std::chrono::milliseconds(config_.interval_sr_ms);
  auto next_rr = std::chrono::steady_clock::now() +
                 std::chrono::milliseconds(config_.interval_rr_ms);
  auto next_sdes = std::chrono::steady_clock::now() +
                   std::chrono::milliseconds(config_.interval_sdes_ms);

  while (!timer_stop_.load()) {
    auto now = std::chrono::steady_clock::now();

    // Check if it's time to send SR
    if (now >= next_sr && config_.enable_sr) {
      SendSr();
      next_sr = now + std::chrono::milliseconds(config_.interval_sr_ms);
    }

    // Check if it's time to send RR
    if (now >= next_rr && config_.enable_rr) {
      SendRr();
      next_rr = now + std::chrono::milliseconds(config_.interval_rr_ms);
    }

    // Check if it's time to send SDES
    if (now >= next_sdes && config_.enable_sdes) {
      // SDES would be sent here
      next_sdes = now + std::chrono::milliseconds(config_.interval_sdes_ms);
    }

    // Sleep until next event
    auto min_time = std::min({next_sr, next_rr, next_sdes});
    if (now < min_time) {
      std::unique_lock<std::mutex> lock(mutex_);
      timer_cv_.wait_for(lock, min_time - now, [this]() {
        return timer_stop_.load();
      });
    }
  }
}

void RTCPModule::ProcessCompoundPacket(const RtcpCompoundPacket& compound,
                                        const NetworkAddress& from) {
  for (const auto& packet : compound.GetPackets()) {
    switch (packet->GetType()) {
      case RtcpPacketType::kSR: {
        auto sr = std::dynamic_pointer_cast<RtcpSrPacket>(packet);
        if (sr) {
          ProcessSenderReport(*sr, from);
        }
        break;
      }
      case RtcpPacketType::kRR: {
        auto rr = std::dynamic_pointer_cast<RtcpRrPacket>(packet);
        if (rr) {
          ProcessReceiverReport(*rr, from);
        }
        break;
      }
      case RtcpPacketType::kRTPFB: {
        auto nack = std::dynamic_pointer_cast<RtcpNackPacket>(packet);
        if (nack) {
          ProcessNack(*nack);
        }
        break;
      }
      case RtcpPacketType::kBYE: {
        auto bye = std::dynamic_pointer_cast<RtcpByePacket>(packet);
        if (bye) {
          ProcessBye(*bye);
        }
        break;
      }
      default:
        break;
    }
  }
}

void RTCPModule::ProcessSenderReport(const RtcpSrPacket& sr,
                                      const NetworkAddress& from) {
  stats_.sr_received++;
  stats_.last_sr_timestamp = sr.GetNtpTimestampUs();
  stats_.last_sr_time_us = GetCurrentTimeUs();

  // Calculate RTT if possible
  uint32_t ntp_high = sr.GetNtpTimestampHigh();
  uint32_t ntp_low = sr.GetNtpTimestampLow();
  if (ntp_high != 0 || ntp_low != 0) {
    // Use NTP timestamp for RTT calculation (simplified)
    uint32_t now = static_cast<uint32_t>(GetCurrentTimeUs() / 1000);
    // Simplified RTT estimation
    stats_.avg_rtt_ms = 50;  // Default estimate
  }

  if (callback_) {
    callback_->OnSenderReportReceived(
        sr.GetSsrc(),
        (static_cast<uint64_t>(sr.GetNtpTimestampHigh()) << 32) |
            sr.GetNtpTimestampLow(),
        sr.GetRtpTimestamp(),
        sr.GetPacketCount(),
        sr.GetOctetCount());
  }
}

void RTCPModule::ProcessReceiverReport(const RtcpRrPacket& rr,
                                        const NetworkAddress& from) {
  stats_.rr_received++;
  stats_.last_report_blocks = rr.GetReportBlocks();

  if (callback_) {
    callback_->OnReceiverReportReceived(rr.GetSsrc(), rr.GetReportBlocks());
  }
}

void RTCPModule::ProcessNack(const RtcpNackPacket& nack) {
  stats_.nack_received++;

  if (callback_) {
    callback_->OnNackRequested(nack.GetNackList());
  }
}

void RTCPModule::ProcessBye(const RtcpByePacket& bye) {
  if (callback_) {
    callback_->OnByeReceived(bye.GetSsrc());
  }
}

std::shared_ptr<RtcpCompoundPacket> RTCPModule::BuildSrCompound() {
  auto compound = CreateRtcpCompound();

  // Build SR
  auto sr = CreateRtcpSr(local_ssrc_);
  sr->SetNtpTimestamp(ntp_timestamp_high_, ntp_timestamp_low_);
  sr->SetRtpTimestamp(last_sender_timestamp_);
  sr->SetPacketCount(sender_packet_count_);
  sr->SetOctetCount(sender_octet_count_);

  // Add report blocks for received packets
  if (config_.enable_rr && total_received_ > 0) {
    RtcpReportBlock block;
    block.ssrc = remote_ssrc_;
    block.fraction_lost = 0;  // Calculate fraction lost
    block.packets_lost = static_cast<int32_t>(total_expected_ - total_received_);
    block.highest_seq = received_seq_;
    block.jitter = jitter_;
    block.lsr = static_cast<uint32_t>(stats_.last_sr_timestamp >> 16);
    block.dlsr = 0;
    sr->AddReportBlock(block);
  }

  compound->AddPacket(sr);

  // Build SDES if enabled
  if (config_.enable_sdes) {
    auto sdes = CreateRtcpSdes();
    sdes->SetCname(local_ssrc_, cname_);
    sdes->SetName(local_ssrc_, name_);
    compound->AddPacket(sdes);
  }

  return compound;
}

std::shared_ptr<RtcpCompoundPacket> RTCPModule::BuildRrCompound() {
  auto compound = CreateRtcpCompound();

  // Build RR
  auto rr = CreateRtcpRr(local_ssrc_);

  // Add report blocks
  if (total_received_ > 0) {
    RtcpReportBlock block;
    block.ssrc = remote_ssrc_;
    block.fraction_lost = total_expected_ > 0
        ? static_cast<uint8_t>(((total_expected_ - total_received_) * 256) /
                                total_expected_)
        : 0;
    block.packets_lost = static_cast<int32_t>(total_expected_ - total_received_);
    block.highest_seq = received_seq_;
    block.jitter = jitter_;
    block.lsr = static_cast<uint32_t>(stats_.last_sr_timestamp >> 16);
    block.dlsr = 0;
    rr->AddReportBlock(block);
  }

  compound->AddPacket(rr);

  // Build SDES if enabled
  if (config_.enable_sdes) {
    auto sdes = CreateRtcpSdes();
    sdes->SetCname(local_ssrc_, cname_);
    compound->AddPacket(sdes);
  }

  return compound;
}

void RTCPModule::GetNtpTimestamp(uint32_t* high, uint32_t* low) {
  auto now = std::chrono::system_clock::now();
  auto epoch = now.time_since_epoch();
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();

  // NTP epoch is 1970-01-01 00:00:00, same as Unix epoch
  *high = static_cast<uint32_t>(seconds);
  *low = 0;
}

uint64_t RTCPModule::GetCurrentTimeUs() {
  auto now = std::chrono::system_clock::now();
  auto epoch = now.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(epoch).count();
}

// ============================================================================
// Factory
// ============================================================================

std::shared_ptr<IRTCPModule> CreateRTCPModule() {
  return std::make_shared<RTCPModule>();
}

}  // namespace minirtc
