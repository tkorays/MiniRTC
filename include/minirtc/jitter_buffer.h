#ifndef MINIRTC_JITTER_BUFFER_H_
#define MINIRTC_JITTER_BUFFER_H_

#include <memory>
#include <queue>
#include <mutex>
#include "minirtc/transport/rtp_packet.h"

namespace minirtc {

// Jitter Buffer模式
enum class JitterBufferMode {
    kPassthrough,   // 透传模式
    kFixed,         // 固定延迟模式
    kAdaptive       // 自适应模式
};

// Jitter Buffer配置
struct JitterBufferConfig {
    JitterBufferMode mode = JitterBufferMode::kPassthrough;
    int fixed_delay_ms = 60;      // 固定延迟模式下的延迟（毫秒）
    int max_delay_ms = 200;       // 最大延迟（毫秒）
    int min_delay_ms = 20;        // 最小延迟（毫秒）
};

// Jitter Buffer统计
struct JitterBufferStats {
    uint64_t packets_in = 0;
    uint64_t packets_out = 0;
    uint64_t packets_dropped = 0;
};

// Jitter Buffer统计（扩展）
struct JitterBufferStatsEx {
    uint64_t packets_in = 0;
    uint64_t packets_out = 0;
    uint64_t packets_dropped = 0;
    uint64_t packets_reorder = 0;
    int current_delay_ms = 0;
    float jitter_ms = 0.0f;
    uint32_t buffer_size = 0;
};

// IJitterBuffer 接口
class IJitterBuffer {
public:
    using Ptr = std::shared_ptr<IJitterBuffer>;
    virtual ~IJitterBuffer() = default;
    virtual bool Initialize(const JitterBufferConfig& config) = 0;
    virtual void Stop() = 0;
    virtual void AddPacket(std::shared_ptr<RtpPacket> packet) = 0;
    virtual std::shared_ptr<RtpPacket> GetPacket(int timeout_ms) = 0;
    virtual JitterBufferStats GetStats() const = 0;
    
    // 设置模式
    virtual void SetMode(JitterBufferMode mode) = 0;
    
    // 设置固定延迟（毫秒）
    virtual void SetFixedDelay(int delay_ms) = 0;
    
    // 设置最大延迟
    virtual void SetMaxDelay(int max_delay_ms) = 0;
    
    // 获取扩展统计
    virtual JitterBufferStatsEx GetStatsEx() const = 0;
};

// 工厂函数
std::shared_ptr<IJitterBuffer> CreateJitterBuffer();
std::shared_ptr<IJitterBuffer> CreateJitterBuffer(JitterBufferMode mode);

}  // namespace minirtc

#endif  // MINIRTC_JITTER_BUFFER_H_
