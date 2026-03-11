#include "minirtc/bandwidth_estimator.h"

#include <algorithm>
#include <cmath>
#include <deque>

namespace minirtc {

// GCC (Google Congestion Control) 简化版实现
// 包含两部分：1) 基于丢包的拥塞控制 2) 基于延迟的拥塞控制

class BandwidthEstimator : public IBandwidthEstimator {
public:
    BandwidthEstimator() = default;
    ~BandwidthEstimator() override = default;

    void Initialize(const BweConfig& config) override {
        config_ = config;
        Reset();
    }

    void OnPacketFeedback(const PacketFeedback& feedback) override {
        int64_t now_ms = feedback.arrival_time_ms;
        
        // 记录包信息用于丢包率计算
        if (feedback.received) {
            received_packets_.push_back({feedback.sequence_number, now_ms, feedback.payload_size});
        } else {
            lost_packets_.push_back({feedback.sequence_number, now_ms});
        }
        
        // 清理过期的包记录（保留1秒窗口）
        CleanOldRecords(now_ms);
        
        // 更新丢包率
        UpdateLossRate(now_ms);
    }

    void OnRttUpdate(int64_t rtt_ms) override {
        // 使用指数移动平均滤波RTT
        if (rtt_ms > 0) {
            if (rtt_count_ == 0) {
                filtered_rtt_ = rtt_ms;
            } else {
                // 简单的IIR滤波: alpha = 0.25
                filtered_rtt_ = static_cast<int64_t>(0.25 * rtt_ms + 0.75 * filtered_rtt_);
            }
            rtt_count_++;
        }
        
        // 更新延迟检测
        UpdateDelayDetection(rtt_ms);
    }

    BweResult GetResult() const override {
        BweResult result;
        result.bitrate_bps = estimated_bitrate_;
        result.target_bitrate_bps = target_bitrate_;
        result.loss_rate = loss_rate_;
        result.rtt_ms = filtered_rtt_;
        return result;
    }

    void Reset() override {
        estimated_bitrate_ = config_.start_bitrate_bps;
        target_bitrate_ = config_.start_bitrate_bps;
        loss_rate_ = 0.0f;
        filtered_rtt_ = 0;
        rtt_count_ = 0;
        
        // 丢包控制状态
        loss_based_state_ = kBandwidthStable;
        last_increase_time_ = 0;
        consecutive_increase_ = 0;
        
        // 延迟检测状态
        delay_based_state_ = kDelayBasedIdle;
        overuse_count_ = 0;
        underuse_count_ = 0;
        
        // 码率平滑
        rate_smoother_.clear();
        
        // 包记录
        received_packets_.clear();
        lost_packets_.clear();
        total_packets_ = 0;
    }

private:
    // 丢包控制状态机
    enum LossBasedState {
        kBandwidthStable,      // 带宽稳定
        kBandwidthIncreasing,  // 带宽增长
        kBandwidthDecreasing   // 带宽下降
    };
    
    // 延迟控制状态机
    enum DelayBasedState {
        kDelayBasedIdle,       // 空闲
        kDelayBasedHold,       // 保持
        kDelayBasedIncrease,   // 增长
        kDelayBasedDecrease,   // 下降
        kDelayBasedOveruse     // 过度使用
    };
    
    struct ReceivedPacket {
        uint16_t seq;
        int64_t time_ms;
        size_t size;
    };
    
    struct LostPacket {
        uint16_t seq;
        int64_t time_ms;
    };

    BweConfig config_;
    
    // 估计值
    uint32_t estimated_bitrate_ = 0;
    uint32_t target_bitrate_ = 0;
    float loss_rate_ = 0.0f;
    int64_t filtered_rtt_ = 0;
    int64_t rtt_count_ = 0;
    
    // 丢包控制
    LossBasedState loss_based_state_ = kBandwidthStable;
    int64_t last_increase_time_ = 0;
    int consecutive_increase_ = 0;
    
    // 延迟控制
    DelayBasedState delay_based_state_ = kDelayBasedIdle;
    int overuse_count_ = 0;
    int underuse_count_ = 0;
    std::deque<int64_t> inter_arrival_deltas_; // 到达间隔delta
    std::deque<int64_t> send_deltas_;          // 发送间隔delta
    
    // 码率平滑队列
    std::deque<std::pair<int64_t, uint32_t>> rate_smoother_; // (time_ms, bitrate)
    
    // 包记录
    std::deque<ReceivedPacket> received_packets_;
    std::deque<LostPacket> lost_packets_;
    int64_t total_packets_ = 0;
    
    // 常量
    static constexpr int64_t kRateWindowMs = 1000;      // 码率窗口1秒
    static constexpr int64_t kFeedbackWindowMs = 1000; // 反馈窗口1秒
    static constexpr float kLossThreshold = 0.02f;     // 丢包率阈值2%
    static constexpr float kHighLossThreshold = 0.1f;  // 高丢包率阈值10%
    static constexpr float kRateIncreaseFactor = 1.05f; // 增长率
    static constexpr float kRateDecreaseFactor = 0.85f; // 下降率
    
    void CleanOldRecords(int64_t now_ms) {
        // 清理超过1秒的接收记录
        while (!received_packets_.empty() && 
               now_ms - received_packets_.front().time_ms > kFeedbackWindowMs) {
            received_packets_.pop_front();
        }
        
        // 清理超过1秒的丢包记录
        while (!lost_packets_.empty() && 
               now_ms - lost_packets_.front().time_ms > kFeedbackWindowMs) {
            lost_packets_.pop_front();
        }
        
        // 清理码率平滑队列
        while (!rate_smoother_.empty() && 
               now_ms - rate_smoother_.front().first > kRateWindowMs) {
            rate_smoother_.pop_front();
        }
    }
    
    void UpdateLossRate(int64_t now_ms) {
        // 计算过去1秒内的丢包率
        int64_t received_count = received_packets_.size();
        int64_t lost_count = lost_packets_.size();
        total_packets_ = received_count + lost_count;
        
        if (total_packets_ > 0) {
            loss_rate_ = static_cast<float>(lost_count) / static_cast<float>(total_packets_);
        } else {
            loss_rate_ = 0.0f;
        }
        
        // 根据丢包率更新码率
        UpdateBitrateFromLoss(now_ms);
    }
    
    void UpdateBitrateFromLoss(int64_t now_ms) {
        switch (loss_based_state_) {
            case kBandwidthStable:
                if (loss_rate_ > kHighLossThreshold) {
                    // 高丢包率：快速降低码率
                    target_bitrate_ = static_cast<uint32_t>(
                        estimated_bitrate_ * kRateDecreaseFactor);
                    target_bitrate_ = std::max(target_bitrate_, config_.min_bitrate_bps);
                    loss_based_state_ = kBandwidthDecreasing;
                } else if (loss_rate_ < kLossThreshold && 
                           now_ms - last_increase_time_ > config_.feedback_interval_ms) {
                    // 低丢包率：增加码率
                    target_bitrate_ = static_cast<uint32_t>(
                        estimated_bitrate_ * kRateIncreaseFactor);
                    target_bitrate_ = std::min(target_bitrate_, config_.max_bitrate_bps);
                    loss_based_state_ = kBandwidthIncreasing;
                    last_increase_time_ = now_ms;
                    consecutive_increase_++;
                }
                break;
                
            case kBandwidthIncreasing:
                if (loss_rate_ > kHighLossThreshold) {
                    // 丢包率过高，回退
                    target_bitrate_ = static_cast<uint32_t>(
                        estimated_bitrate_ * kRateDecreaseFactor);
                    target_bitrate_ = std::max(target_bitrate_, config_.min_bitrate_bps);
                    loss_based_state_ = kBandwidthDecreasing;
                    consecutive_increase_ = 0;
                } else if (loss_rate_ > kLossThreshold) {
                    // 丢包率超过阈值，停止增长
                    loss_based_state_ = kBandwidthStable;
                    consecutive_increase_ = 0;
                } else if (now_ms - last_increase_time_ > config_.feedback_interval_ms) {
                    // 继续增长
                    target_bitrate_ = static_cast<uint32_t>(
                        target_bitrate_ * kRateIncreaseFactor);
                    target_bitrate_ = std::min(target_bitrate_, config_.max_bitrate_bps);
                    last_increase_time_ = now_ms;
                    consecutive_increase_++;
                }
                break;
                
            case kBandwidthDecreasing:
                // 持续降低后恢复稳定
                if (loss_rate_ < kLossThreshold) {
                    loss_based_state_ = kBandwidthStable;
                }
                break;
        }
    }
    
    void UpdateDelayDetection(int64_t rtt_ms) {
        // 简化的基于延迟的拥塞控制
        // 如果RTT显著增加，检测到可能的拥塞
        
        if (rtt_ms <= 0 || rtt_count_ < 2) {
            return;
        }
        
        // 计算RTT变化
        int64_t rtt_variance = rtt_ms - filtered_rtt_;
        
        if (rtt_variance > config_.rtt_filter_ms / 2) {
            // RTT显著增加，可能是拥塞
            overuse_count_++;
            underuse_count_ = 0;
            
            if (overuse_count_ >= 3) {
                // 连续3次检测到 overuse，降低码率
                delay_based_state_ = kDelayBasedOveruse;
                
                // 基于延迟的码率调整
                uint32_t delay_reduction = static_cast<uint32_t>(
                    estimated_bitrate_ * 0.1f * (rtt_variance / 100.0f));
                target_bitrate_ = std::max(
                    target_bitrate_ - delay_reduction, 
                    config_.min_bitrate_bps);
                
                overuse_count_ = 0;
            }
        } else if (rtt_variance < -config_.rtt_filter_ms / 4) {
            // RTT显著降低，网络空闲
            underuse_count_++;
            overuse_count_ = 0;
            
            if (underuse_count_ >= 3 && loss_based_state_ != kBandwidthIncreasing) {
                // 连续3次检测到 underuse，可以尝试增加码率
                delay_based_state_ = kDelayBasedIncrease;
                underuse_count_ = 0;
            }
        } else {
            // RTT稳定
            overuse_count_ = std::max(0, overuse_count_ - 1);
            underuse_count_ = std::max(0, underuse_count_ - 1);
            
            if (overuse_count_ == 0 && underuse_count_ == 0) {
                delay_based_state_ = kDelayBasedIdle;
            }
        }
        
        // 应用基于延迟的调整到目标码率
        ApplyDelayBasedAdjustment();
    }
    
    void ApplyDelayBasedAdjustment() {
        // 结合丢包控制和延迟控制的结果
        // 取两者中较小的值，确保更保守的估计
        
        if (delay_based_state_ == kDelayBasedOveruse) {
            // 延迟检测到拥塞，确保码率不超过当前估计
            target_bitrate_ = std::min(target_bitrate_, estimated_bitrate_);
        } else if (delay_based_state_ == kDelayBasedIncrease && 
                   loss_based_state_ == kBandwidthStable) {
            // 延迟和丢包都表明可以增加
            if (consecutive_increase_ < 2) {
                target_bitrate_ = static_cast<uint32_t>(
                    target_bitrate_ * 1.02f);  // 缓慢增加
                target_bitrate_ = std::min(target_bitrate_, config_.max_bitrate_bps);
            }
        }
    }
    
    void UpdateSmoothedBitrate(int64_t now_ms, uint32_t new_bitrate) {
        // 添加到平滑队列
        rate_smoother_.push_back({now_ms, new_bitrate});
        
        // 计算加权平均（更重视最近的样本）
        if (rate_smoother_.size() > 1) {
            uint64_t weighted_sum = 0;
            uint64_t weight_total = 0;
            
            for (size_t i = 0; i < rate_smoother_.size(); ++i) {
                uint64_t weight = i + 1;  // 越近权重越大
                weighted_sum += rate_smoother_[i].second * weight;
                weight_total += weight;
            }
            
            estimated_bitrate_ = static_cast<uint32_t>(weighted_sum / weight_total);
        } else {
            estimated_bitrate_ = new_bitrate;
        }
        
        // 限制范围
        estimated_bitrate_ = std::max(estimated_bitrate_, config_.min_bitrate_bps);
        estimated_bitrate_ = std::min(estimated_bitrate_, config_.max_bitrate_bps);
    }
};

// 创建带宽估计器实例
std::shared_ptr<IBandwidthEstimator> CreateBandwidthEstimator() {
    return std::make_shared<BandwidthEstimator>();
}

}  // namespace minirtc
