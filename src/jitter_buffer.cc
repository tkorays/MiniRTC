/**
 * @file jitter_buffer.cc
 * @brief Jitter Buffer implementation - PassThrough, Fixed and Adaptive modes
 */

#include "minirtc/jitter_buffer.h"
#include <chrono>
#include <condition_variable>
#include <map>
#include <cmath>

namespace minirtc {

// RTP包带时间戳的封装
struct TimestampedPacket {
    std::shared_ptr<RtpPacket> packet;
    int64_t receive_time_ms;  // 接收时间戳
};

// ============================================================================
// PassThroughJitterBuffer 实现
// ============================================================================
class PassThroughJitterBuffer : public IJitterBuffer {
public:
    PassThroughJitterBuffer() = default;
    ~PassThroughJitterBuffer() override { Stop(); }

    bool Initialize(const JitterBufferConfig& config) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        running_ = true;
        return true;
    }

    void Stop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        cv_.notify_all();
    }

    void AddPacket(std::shared_ptr<RtpPacket> packet) override {
        if (!packet) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            stats_.packets_dropped++;
            return;
        }

        // 透传模式：使用队列缓冲，避免丢包
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        pending_packets_.push({packet, now});
        stats_.packets_in++;
        cv_.notify_one();
    }

    std::shared_ptr<RtpPacket> GetPacket(int timeout_ms) override {
        std::unique_lock<std::mutex> lock(mutex_);

        if (cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
            return !pending_packets_.empty() || !running_;
        })) {
            if (!pending_packets_.empty()) {
                auto item = pending_packets_.front();
                pending_packets_.pop();
                stats_.packets_out++;
                return item.packet;
            }
        }

        return nullptr;
    }

    JitterBufferStats GetStats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    void SetMode(JitterBufferMode mode) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.mode = mode;
    }

    void SetFixedDelay(int delay_ms) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.fixed_delay_ms = delay_ms;
    }

    void SetMaxDelay(int max_delay_ms) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.max_delay_ms = max_delay_ms;
    }

    JitterBufferStatsEx GetStatsEx() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        JitterBufferStatsEx ex;
        ex.packets_in = stats_.packets_in;
        ex.packets_out = stats_.packets_out;
        ex.packets_dropped = stats_.packets_dropped;
        ex.buffer_size = static_cast<uint32_t>(pending_packets_.size());
        return ex;
    }

private:
    mutable JitterBufferConfig config_;
    mutable JitterBufferStats stats_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<TimestampedPacket> pending_packets_;
    bool running_ = false;
};

// ============================================================================
// FixedJitterBuffer 实现 - 固定延迟模式
// ============================================================================
class FixedJitterBuffer : public IJitterBuffer {
public:
    FixedJitterBuffer() = default;
    ~FixedJitterBuffer() override { Stop(); }

    bool Initialize(const JitterBufferConfig& config) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        running_ = true;
        return true;
    }

    void Stop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        cv_.notify_all();
    }

    void AddPacket(std::shared_ptr<RtpPacket> packet) override {
        if (!packet) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            stats_.packets_dropped++;
            return;
        }

        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        // 按序列号插入，保持有序
        uint16_t seq = packet->GetSequenceNumber();
        buffer_[seq] = {packet, now};
        stats_.packets_in++;
        
        // 通知可能有包可以取出
        cv_.notify_one();
        
        // 清理过期的包
        CleanupLocked(now);
    }

    std::shared_ptr<RtpPacket> GetPacket(int timeout_ms) override {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 先检查是否有可取的包（已到期）
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        int64_t target_time = now - config_.fixed_delay_ms;
        
        // 检查第一个包是否到期
        if (!buffer_.empty()) {
            auto& first = buffer_.begin()->second;
            if (first.receive_time_ms <= target_time) {
                auto packet = first.packet;
                buffer_.erase(buffer_.begin());
                stats_.packets_out++;
                return packet;
            }
        }

        // 等待新包或超时
        if (cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
            return !running_;
        })) {
            // 尝试获取到期的包
            now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            target_time = now - config_.fixed_delay_ms;
            
            if (!buffer_.empty()) {
                auto& first = buffer_.begin()->second;
                if (first.receive_time_ms <= target_time) {
                    auto packet = first.packet;
                    buffer_.erase(buffer_.begin());
                    stats_.packets_out++;
                    return packet;
                }
            }
        }

        return nullptr;
    }

    JitterBufferStats GetStats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    void SetMode(JitterBufferMode mode) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.mode = mode;
    }

    void SetFixedDelay(int delay_ms) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.fixed_delay_ms = delay_ms;
    }

    void SetMaxDelay(int max_delay_ms) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.max_delay_ms = max_delay_ms;
    }

    JitterBufferStatsEx GetStatsEx() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        JitterBufferStatsEx ex;
        ex.packets_in = stats_.packets_in;
        ex.packets_out = stats_.packets_out;
        ex.packets_dropped = stats_.packets_dropped;
        ex.buffer_size = static_cast<uint32_t>(buffer_.size());
        ex.current_delay_ms = config_.fixed_delay_ms;
        return ex;
    }

private:
    void CleanupLocked(int64_t now) {
        int64_t expire_time = now - config_.max_delay_ms;
        for (auto it = buffer_.begin(); it != buffer_.end();) {
            if (it->second.receive_time_ms < expire_time) {
                stats_.packets_dropped++;
                it = buffer_.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::shared_ptr<RtpPacket> GetNextPacketLocked(int64_t now) {
        if (buffer_.empty()) {
            return nullptr;
        }

        // 找到最早可以交付的包（已到达延迟时间）
        int64_t target_time = now - config_.fixed_delay_ms;
        
        for (auto& kv : buffer_) {
            if (kv.second.receive_time_ms <= target_time) {
                auto packet = kv.second.packet;
                buffer_.erase(kv.first);
                stats_.packets_out++;
                cv_.notify_one();  // 通知可能有更多包准备好
                return packet;
            }
        }

        return nullptr;
    }

    mutable JitterBufferConfig config_;
    mutable JitterBufferStats stats_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::map<uint16_t, TimestampedPacket> buffer_;  // 按序列号排序的缓冲区
    bool running_ = false;
};

// ============================================================================
// AdaptiveJitterBuffer 实现 - 自适应模式
// ============================================================================
class AdaptiveJitterBuffer : public IJitterBuffer {
public:
    AdaptiveJitterBuffer() 
        : current_delay_ms_(60),
          jitter_ms_(0.0f),
          last_seq_(0),
          last_timestamp_(0),
          last_receive_time_ms_(0),
          packets_since_last_adjust_(0),
          consecutive_drops_(0) {
    }
    ~AdaptiveJitterBuffer() override { Stop(); }

    bool Initialize(const JitterBufferConfig& config) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        current_delay_ms_ = config.fixed_delay_ms > 0 ? config.fixed_delay_ms : 60;
        running_ = true;
        return true;
    }

    void Stop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        cv_.notify_all();
    }

    void AddPacket(std::shared_ptr<RtpPacket> packet) override {
        if (!packet) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            stats_.packets_dropped++;
            return;
        }

        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        uint16_t seq = packet->GetSequenceNumber();
        uint32_t timestamp = packet->GetTimestamp();

        // 检测丢包
        if (last_seq_ > 0) {
            int16_t seq_diff = static_cast<int16_t>(seq - last_seq_);
            if (seq_diff < 0) {
                // 序列号回绕或丢包
                if (seq_diff < -100) {
                    // 真正的丢包
                    consecutive_drops_++;
                    AdjustDelayOnPacketLossLocked();
                }
            } else if (seq_diff > 1) {
                // 丢包 detected
                consecutive_drops_ += (seq_diff - 1);
                AdjustDelayOnPacketLossLocked();
            } else {
                // 正常包，重置连续丢包计数
                consecutive_drops_ = 0;
            }
        }

        // 计算jitter
        if (last_receive_time_ms_ > 0 && last_timestamp_ > 0) {
            // 估算包间隔（假设8kHz采样率，1个样本=0.125ms）
            uint32_t timestamp_diff = timestamp - last_timestamp_;
            int64_t time_diff = now - last_receive_time_ms_;
            int64_t expected_time_diff = timestamp_diff * 125 / 1000;  // 转换为毫秒
            
            if (expected_time_diff > 0) {
                int64_t jitter_delta = std::llabs(time_diff - expected_time_diff);
                // 简单的指数移动平均
                jitter_ms_ = jitter_ms_ * 0.9f + static_cast<float>(jitter_delta) * 0.1f;
            }
        }

        last_seq_ = seq;
        last_timestamp_ = timestamp;
        last_receive_time_ms_ = now;

        // 插入缓冲区
        buffer_[seq] = {packet, now};
        stats_.packets_in++;

        packets_since_last_adjust_++;

        // 周期性调整延迟
        if (packets_since_last_adjust_ >= 50) {
            AdjustDelayLocked();
            packets_since_last_adjust_ = 0;
        }

        // 清理过期的包
        CleanupLocked(now);
        
        cv_.notify_one();
    }

    std::shared_ptr<RtpPacket> GetPacket(int timeout_ms) override {
        std::unique_lock<std::mutex> lock(mutex_);

        if (cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
            return !running_;
        })) {
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            
            return GetNextPacketLocked(now);
        }

        return nullptr;
    }

    JitterBufferStats GetStats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    void SetMode(JitterBufferMode mode) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.mode = mode;
    }

    void SetFixedDelay(int delay_ms) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.fixed_delay_ms = delay_ms;
        current_delay_ms_ = delay_ms;
    }

    void SetMaxDelay(int max_delay_ms) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.max_delay_ms = max_delay_ms;
    }

    JitterBufferStatsEx GetStatsEx() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        JitterBufferStatsEx ex;
        ex.packets_in = stats_.packets_in;
        ex.packets_out = stats_.packets_out;
        ex.packets_dropped = stats_.packets_dropped;
        ex.packets_reorder = 0;  // 可扩展
        ex.current_delay_ms = current_delay_ms_;
        ex.jitter_ms = jitter_ms_;
        ex.buffer_size = static_cast<uint32_t>(buffer_.size());
        return ex;
    }

private:
    void AdjustDelayLocked() {
        int target_delay = config_.min_delay_ms;
        
        // 基于jitter计算目标延迟: 4 * jitter + 基础延迟
        int jitter_based_delay = static_cast<int>(jitter_ms_ * 4);
        if (jitter_based_delay < config_.min_delay_ms) {
            jitter_based_delay = config_.min_delay_ms;
        }
        
        target_delay = std::max(jitter_based_delay, target_delay);
        
        // 如果有丢包，增加延迟
        if (consecutive_drops_ > 0) {
            target_delay += consecutive_drops_ * 20;
        }
        
        // 限制最大延迟
        if (target_delay > config_.max_delay_ms) {
            target_delay = config_.max_delay_ms;
        }
        
        // 逐步调整，避免突变
        if (target_delay > current_delay_ms_) {
            current_delay_ms_ += (target_delay - current_delay_ms_) / 4;
        } else if (target_delay < current_delay_ms_) {
            current_delay_ms_ -= (current_delay_ms_ - target_delay) / 8;
        }
        
        // 确保在合理范围内
        if (current_delay_ms_ < config_.min_delay_ms) {
            current_delay_ms_ = config_.min_delay_ms;
        }
        if (current_delay_ms_ > config_.max_delay_ms) {
            current_delay_ms_ = config_.max_delay_ms;
        }
    }

    void AdjustDelayOnPacketLossLocked() {
        // 丢包时增加缓冲，减少进一步丢包
        current_delay_ms_ += 20;
        if (current_delay_ms_ > config_.max_delay_ms) {
            current_delay_ms_ = config_.max_delay_ms;
        }
    }

    void CleanupLocked(int64_t now) {
        int64_t expire_time = now - config_.max_delay_ms;
        for (auto it = buffer_.begin(); it != buffer_.end();) {
            if (it->second.receive_time_ms < expire_time) {
                stats_.packets_dropped++;
                it = buffer_.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::shared_ptr<RtpPacket> GetNextPacketLocked(int64_t now) {
        if (buffer_.empty()) {
            return nullptr;
        }

        int64_t target_time = now - current_delay_ms_;
        
        for (auto& kv : buffer_) {
            if (kv.second.receive_time_ms <= target_time) {
                auto packet = kv.second.packet;
                buffer_.erase(kv.first);
                stats_.packets_out++;
                cv_.notify_one();
                return packet;
            }
        }

        return nullptr;
    }

    mutable JitterBufferConfig config_;
    mutable JitterBufferStats stats_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::map<uint16_t, TimestampedPacket> buffer_;
    bool running_ = false;
    
    // 自适应相关状态
    int current_delay_ms_;
    float jitter_ms_;
    uint16_t last_seq_;
    uint32_t last_timestamp_;
    int64_t last_receive_time_ms_;
    int packets_since_last_adjust_;
    int consecutive_drops_;
};

// ============================================================================
// 工厂函数
// ============================================================================
std::shared_ptr<IJitterBuffer> CreateJitterBuffer(JitterBufferMode mode) {
    switch (mode) {
        case JitterBufferMode::kFixed:
            return std::make_shared<FixedJitterBuffer>();
        case JitterBufferMode::kAdaptive:
            return std::make_shared<AdaptiveJitterBuffer>();
        case JitterBufferMode::kPassthrough:
        default:
            return std::make_shared<PassThroughJitterBuffer>();
    }
}

// 兼容旧接口：默认创建透传模式
std::shared_ptr<IJitterBuffer> CreateJitterBuffer() {
    return CreateJitterBuffer(JitterBufferMode::kPassthrough);
}

}  // namespace minirtc
