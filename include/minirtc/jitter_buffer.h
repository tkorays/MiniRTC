#ifndef MINIRTC_JITTER_BUFFER_H_
#define MINIRTC_JITTER_BUFFER_H_

#include <memory>
#include <queue>
#include <mutex>
#include "minirtc/transport/rtp_packet.h"

namespace minirtc {

// Jitter Buffer配置
struct JitterBufferConfig {
    bool passthrough_mode = true;  // 透传模式
    int max_buffer_ms = 0;         // 透传模式下忽略
};

// Jitter Buffer统计
struct JitterBufferStats {
    uint64_t packets_in = 0;
    uint64_t packets_out = 0;
    uint64_t packets_dropped = 0;
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
};

// 工厂函数
std::shared_ptr<IJitterBuffer> CreateJitterBuffer();

}  // namespace minirtc

#endif  // MINIRTC_JITTER_BUFFER_H_
