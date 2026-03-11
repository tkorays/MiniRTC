#ifndef MINIRTC_BANDWIDTH_ESTIMATOR_H_
#define MINIRTC_BANDWIDTH_ESTIMATOR_H_

#include <memory>
#include <stdint.h>
#include <vector>

namespace minirtc {

// 带宽估计配置
struct BweConfig {
    uint32_t min_bitrate_bps = 30000;      // 最小码率 30kbps
    uint32_t max_bitrate_bps = 3000000;    // 最大码率 3Mbps
    uint32_t start_bitrate_bps = 300000;   // 初始码率 300kbps
    int64_t feedback_interval_ms = 100;    // 反馈间隔
    int64_t rtt_filter_ms = 200;           // RTT滤波
};

// 带宽估计结果
struct BweResult {
    uint32_t bitrate_bps = 0;              // 估计带宽
    uint32_t target_bitrate_bps = 0;       // 目标码率
    float loss_rate = 0.0f;                // 丢包率
    int64_t rtt_ms = 0;                    // RTT
};

// 丢包检测
struct PacketFeedback {
    int64_t arrival_time_ms = 0;
    uint32_t send_time_ms = 0;
    uint16_t sequence_number = 0;
    size_t payload_size = 0;
    bool received = true;
};

// IBandwidthEstimator接口
class IBandwidthEstimator {
public:
    using Ptr = std::shared_ptr<IBandwidthEstimator>;
    virtual ~IBandwidthEstimator() = default;
    
    // 初始化
    virtual void Initialize(const BweConfig& config) = 0;
    
    // 传入包反馈（丢包检测）
    virtual void OnPacketFeedback(const PacketFeedback& feedback) = 0;
    
    // 传入RTT
    virtual void OnRttUpdate(int64_t rtt_ms) = 0;
    
    // 获取当前估计
    virtual BweResult GetResult() const = 0;
    
    // 重置
    virtual void Reset() = 0;
};

// 创建带宽估计器 (GCC简化版)
std::shared_ptr<IBandwidthEstimator> CreateBandwidthEstimator();

}  // namespace minirtc

#endif
