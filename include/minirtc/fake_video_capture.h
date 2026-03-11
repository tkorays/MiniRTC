/**
 * @file fake_video_capture.h
 * @brief Fake video capture implementation for testing
 */

#ifndef MINIRTC_FAKE_VIDEO_CAPTURE_H
#define MINIRTC_FAKE_VIDEO_CAPTURE_H

#include "capture_render.h"
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <random>

namespace minirtc {
namespace fake {

// ============================================================================
// Video Frame Generator
// ============================================================================

class VideoFrameGenerator {
public:
    virtual ~VideoFrameGenerator() = default;
    virtual void GenerateFrame(VideoFrame* frame) = 0;
};

class SolidColorGenerator : public VideoFrameGenerator {
public:
    SolidColorGenerator(int width, int height, uint32_t rgba_color);
    void GenerateFrame(VideoFrame* frame) override;

private:
    int width_;
    int height_;
    uint32_t rgba_color_;
};

class GradientGenerator : public VideoFrameGenerator {
public:
    GradientGenerator(int width, int height);
    void GenerateFrame(VideoFrame* frame) override;

private:
    int width_;
    int height_;
};

class CheckerboardGenerator : public VideoFrameGenerator {
public:
    CheckerboardGenerator(int width, int height, int block_size = 32);
    void GenerateFrame(VideoFrame* frame) override;

private:
    int width_;
    int height_;
    int block_size_;
};

// ============================================================================
// Fake Video Capture
// ============================================================================

class FakeVideoCapture : public IVideoCapture {
public:
    FakeVideoCapture();
    ~FakeVideoCapture() override;

    // Configuration
    void SetFrameGenerator(std::unique_ptr<VideoFrameGenerator> generator);
    void SetCaptureLatency(int64_t latency_us);
    void SetDropFrameRate(float rate);
    void SetGenerateSineWave(int frequency_hz = 60);
    void SetSolidColor(uint32_t rgba_color);
    void SetGradient();
    void SetCheckerboard(int block_size = 32);

    // IVideoCapture interface
    ErrorCode GetDevices(std::vector<VideoDeviceInfo>* devices) override;
    ErrorCode SetDevice(const std::string& device_id) override;
    std::string GetCurrentDevice() const override;

    ErrorCode SetParam(const VideoCaptureParam& param) override;
    ErrorCode GetParam(VideoCaptureParam* param) const override;

    ErrorCode Initialize() override;
    ErrorCode StartCapture(std::weak_ptr<VideoCaptureObserver> observer) override;
    ErrorCode StopCapture() override;
    ErrorCode Release() override;

    CaptureState GetState() const override;
    bool IsCapturing() const override;

    ErrorCode GetStats(VideoCaptureStats* stats) const override;
    ErrorCode ResetStats() override;

    ErrorCode SetResolution(int width, int height) override;
    ErrorCode SetFrameRate(int fps) override;
    ErrorCode SetRotation(int32_t rotation) override;
    ErrorCode SetMirror(bool mirror) override;
    ErrorCode SetBrightness(int value) override;
    ErrorCode SetContrast(int value) override;
    ErrorCode SetSaturation(int value) override;

private:
    void GenerateAndPushFrame();
    void CaptureThreadLoop();

    std::weak_ptr<VideoCaptureObserver> observer_;
    std::unique_ptr<VideoFrameGenerator> frame_generator_;
    VideoCaptureParam param_;
    std::string device_id_;

    int64_t capture_latency_us_ = 0;
    float drop_frame_rate_ = 0.0f;

    std::atomic<CaptureState> state_{CaptureState::kIdle};
    std::atomic<bool> capturing_{false};
    std::thread capture_thread_;

    mutable std::mutex stats_mutex_;
    VideoCaptureStats stats_;

    std::atomic<uint16_t> seq_num_{0};
    std::atomic<uint32_t> timestamp_rtp_{0};
    std::atomic<int64_t> start_time_us_{0};
};

}  // namespace fake
}  // namespace minirtc

#endif  // MINIRTC_FAKE_VIDEO_CAPTURE_H
