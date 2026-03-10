/**
 * @file test_nack_module.cc
 * @brief NACK module mock implementation
 */

#include "test_nack_module.h"
#include <algorithm>

namespace minirtc {
namespace test {

MockNackModule::MockNackModule(const NackConfig& config) : config_(config) {}

void MockNackModule::OnRtpPacketReceived(uint16_t seq, uint32_t timestamp) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  stats_.packets_received++;
  
  // 第一次收到包
  if (first_packet_) {
    expected_seq_ = seq;
    first_packet_ = false;
    received_packets_.insert(seq);
    return;
  }
  
  // 检测丢包
  DetectPacketLoss(seq);
  
  // 记录收到的包
  received_packets_.insert(seq);
}

void MockNackModule::DetectPacketLoss(uint16_t seq) {
  // 处理序列号回绕
  int16_t diff = static_cast<int16_t>(seq - expected_seq_);
  
  if (diff > 0) {
    // 有丢包
    for (uint16_t s = expected_seq_; s < seq; ++s) {
      if (received_packets_.find(s) == received_packets_.end() &&
          nack_map_.find(s) == nack_map_.end()) {
        // 新丢包
        NackEntry entry;
        entry.timestamp = 0;  // TODO: 使用实际timestamp
        entry.retransmit_count = 0;
        entry.last_nack_time_ms = 0;
        nack_map_[s] = entry;
        stats_.packets_lost++;
      }
    }
  }
  
  expected_seq_ = seq + 1;
}

std::vector<uint16_t> MockNackModule::GetNackList(uint64_t current_time_ms) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  UpdateNackList(current_time_ms);
  
  std::vector<uint16_t> result;
  for (const auto& kv : nack_map_) {
    result.push_back(kv.first);
  }
  
  return result;
}

void MockNackModule::UpdateNackList(uint64_t current_time_ms) {
  // 更新NACK超时
  auto it = nack_map_.begin();
  while (it != nack_map_.end()) {
    uint16_t seq = it->first;
    NackEntry& entry = it->second;
    
    // 检查是否已恢复
    if (recovered_packets_.find(seq) != recovered_packets_.end()) {
      it = nack_map_.erase(it);
      continue;
    }
    
    // 检查超时
    if (current_time_ms - entry.last_nack_time_ms > config_.nack_timeout_ms) {
      if (entry.retransmit_count >= config_.max_retransmissions) {
        // 超过最大重传次数，移除
        it = nack_map_.erase(it);
        continue;
      }
      
      // 更新重传时间和计数
      entry.last_nack_time_ms = current_time_ms;
      entry.retransmit_count++;
      stats_.nack_requests_sent++;
    }
    
    ++it;
  }
}

void MockNackModule::OnRtxPacketReceived(uint16_t original_seq) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  recovered_packets_.insert(original_seq);
  stats_.packets_recovered++;
  
  // 从NACK列表中移除
  nack_map_.erase(original_seq);
}

MockNackModule::Stats MockNackModule::GetStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return stats_;
}

void MockNackModule::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  expected_seq_ = 0;
  first_packet_ = true;
  nack_map_.clear();
  received_packets_.clear();
  recovered_packets_.clear();
  stats_ = {};
}

void MockNackModule::SetPacketLossCallback(
    std::function<bool(uint16_t)> callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  packet_loss_callback_ = std::move(callback);
}

} // namespace test
} // namespace minirtc
