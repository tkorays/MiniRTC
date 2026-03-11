/**
 * @file jitter_buffer.cc
 * @brief Jitter Buffer implementation - PassThrough mode
 */

#include "minirtc/jitter_buffer.h"
#include <chrono>
#include <condition_variable>

namespace minirtc {

// PassThroughJitterBuffer 实现
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

        if (config_.passthrough_mode) {
            // 透传模式：使用队列缓冲，避免丢包
            pending_packets_.push(packet);
            stats_.packets_in++;
            cv_.notify_one();
        }
    }

    std::shared_ptr<RtpPacket> GetPacket(int timeout_ms) override {
        std::unique_lock<std::mutex> lock(mutex_);

        if (cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
            return !pending_packets_.empty() || !running_;
        })) {
            if (!pending_packets_.empty()) {
                auto packet = pending_packets_.front();
                pending_packets_.pop();
                stats_.packets_out++;
                return packet;
            }
        }

        return nullptr;
    }

    JitterBufferStats GetStats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

private:
    mutable JitterBufferConfig config_;
    mutable JitterBufferStats stats_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::shared_ptr<RtpPacket>> pending_packets_;  // 使用队列替代单个packet
    bool running_ = false;
};

// 工厂函数
std::shared_ptr<IJitterBuffer> CreateJitterBuffer() {
    return std::make_shared<PassThroughJitterBuffer>();
}

}  // namespace minirtc
