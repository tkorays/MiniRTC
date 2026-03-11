/**
 * @file fake_video_capture.cc
 * @brief Fake video capture implementation
 */

#include "minirtc/fake_video_capture.h"

#include <chrono>
#include <cstring>
#include <cmath>
#include <random>

namespace minirtc {
namespace fake {

using namespace std::chrono;

// ============================================================================
// VideoFrameGenerator
// ============================================================================

SolidColorGenerator::SolidColorGenerator(int width, int height, uint32_t rgba_color)
    : width_(width), height_(height), rgba_color_(rgba_color) {}

void SolidColorGenerator::GenerateFrame(VideoFrame* frame) {
    frame->width = width_;
    frame->height = height_;
    frame->format = VideoPixelFormat::kRGBA;
    frame->AllocateBuffer();
    frame->SetStride();

    uint8_t r = (rgba_color_ >> 16) & 0xFF;
    uint8_t g = (rgba_color_ >> 8) & 0xFF;
    uint8_t b = rgba_color_ & 0xFF;

    uint8_t* data = frame->internal_buffer.data();
    for (int i = 0; i < width_ * height_; ++i) {
        data[i * 4 + 0] = r;
        data[i * 4 + 1] = g;
        data[i * 4 + 2] = b;
        data[i * 4 + 3] = 0xFF;
    }

    frame->data_y = frame->internal_buffer.data();
}

GradientGenerator::GradientGenerator(int width, int height)
    : width_(width), height_(height) {}

void GradientGenerator::GenerateFrame(VideoFrame* frame) {
    frame->width = width_;
    frame->height = height_;
    frame->format = VideoPixelFormat::kRGBA;
    frame->AllocateBuffer();
    frame->SetStride();

    uint8_t* data = frame->internal_buffer.data();
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            int idx = (y * width_ + x) * 4;
            data[idx + 0] = static_cast<uint8_t>((x * 255) / width_);
            data[idx + 1] = static_cast<uint8_t>((y * 255) / height_);
            data[idx + 2] = 128;
            data[idx + 3] = 0xFF;
        }
    }

    frame->data_y = frame->internal_buffer.data();
}

CheckerboardGenerator::CheckerboardGenerator(int width, int height, int block_size)
    : width_(width), height_(height), block_size_(block_size) {}

void CheckerboardGenerator::GenerateFrame(VideoFrame* frame) {
    frame->width = width_;
    frame->height = height_;
    frame->format = VideoPixelFormat::kRGBA;
    frame->AllocateBuffer();
    frame->SetStride();

    uint8_t color1[4] = {255, 255, 255, 255};
    uint8_t color2[4] = {0, 0, 0, 255};

    uint8_t* data = frame->internal_buffer.data();
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            int idx = (y * width_ + x) * 4;
            bool is_white = ((x / block_size_) + (y / block_size_)) % 2 == 0;
            uint8_t* color = is_white ? color1 : color2;
            std::memcpy(data + idx, color, 4);
        }
    }

    frame->data_y = frame->internal_buffer.data();
}

// ============================================================================
// FakeVideoCapture
// ============================================================================

FakeVideoCapture::FakeVideoCapture() {
    frame_generator_ = std::make_unique<GradientGenerator>(640, 480);
}

FakeVideoCapture::~FakeVideoCapture() {
    StopCapture();
    Release();
}

void FakeVideoCapture::SetFrameGenerator(std::unique_ptr<VideoFrameGenerator> generator) {
    frame_generator_ = std::move(generator);
}

void FakeVideoCapture::SetCaptureLatency(int64_t latency_us) {
    capture_latency_us_ = latency_us;
}

void FakeVideoCapture::SetDropFrameRate(float rate) {
    drop_frame_rate_ = std::max(0.0f, std::min(1.0f, rate));
}

void FakeVideoCapture::SetGenerateSineWave(int frequency_hz) {
    (void)frequency_hz;
    frame_generator_ = std::make_unique<SolidColorGenerator>(param_.width, param_.height, 0x0080FF);
}

void FakeVideoCapture::SetSolidColor(uint32_t rgba_color) {
    frame_generator_ = std::make_unique<SolidColorGenerator>(
        param_.width > 0 ? param_.width : 640,
        param_.height > 0 ? param_.height : 480,
        rgba_color);
}

void FakeVideoCapture::SetGradient() {
    frame_generator_ = std::make_unique<GradientGenerator>(
        param_.width > 0 ? param_.width : 640,
        param_.height > 0 ? param_.height : 480);
}

void FakeVideoCapture::SetCheckerboard(int block_size) {
    frame_generator_ = std::make_unique<CheckerboardGenerator>(
        param_.width > 0 ? param_.width : 640,
        param_.height > 0 ? param_.height : 480,
        block_size);
}

ErrorCode FakeVideoCapture::GetDevices(std::vector<VideoDeviceInfo>* devices) {
    if (!devices) return ErrorCode::kInvalidParam;

    VideoDeviceInfo info;
    info.device_id = "fake_video_device_0";
    info.device_name = "Fake Video Camera";
    info.unique_id = "fake_video_unique_0";
    info.is_default = true;
    devices->push_back(info);

    return ErrorCode::kOk;
}

ErrorCode FakeVideoCapture::SetDevice(const std::string& device_id) {
    device_id_ = device_id;
    return ErrorCode::kOk;
}

std::string FakeVideoCapture::GetCurrentDevice() const {
    return device_id_.empty() ? "fake_video_device_0" : device_id_;
}

ErrorCode FakeVideoCapture::SetParam(const VideoCaptureParam& param) {
    if (!param.IsValid()) return ErrorCode::kInvalidParam;
    param_ = param;

    if (frame_generator_) {
        frame_generator_ = std::make_unique<GradientGenerator>(param_.width, param_.height);
    }

    return ErrorCode::kOk;
}

ErrorCode FakeVideoCapture::GetParam(VideoCaptureParam* param) const {
    if (!param) return ErrorCode::kInvalidParam;
    *param = param_;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoCapture::Initialize() {
    if (state_.load() != CaptureState::kIdle) {
        return ErrorCode::kAlreadyStarted;
    }

    state_.store(CaptureState::kInitializing);
    std::this_thread::sleep_for(milliseconds(10));
    state_.store(CaptureState::kReady);

    return ErrorCode::kOk;
}

ErrorCode FakeVideoCapture::StartCapture(std::weak_ptr<VideoCaptureObserver> observer) {
    if (state_.load() == CaptureState::kCapturing) {
        return ErrorCode::kAlreadyStarted;
    }

    if (state_.load() != CaptureState::kReady) {
        return ErrorCode::kNotInitialized;
    }

    observer_ = observer;
    capturing_.store(true);
    state_.store(CaptureState::kCapturing);

    start_time_us_ = duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();

    capture_thread_ = std::thread(&FakeVideoCapture::CaptureThreadLoop, this);

    return ErrorCode::kOk;
}

ErrorCode FakeVideoCapture::StopCapture() {
    if (state_.load() != CaptureState::kCapturing) {
        return ErrorCode::kNotStarted;
    }

    state_.store(CaptureState::kStopping);
    capturing_.store(false);

    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }

    state_.store(CaptureState::kReady);
    return ErrorCode::kOk;
}

ErrorCode FakeVideoCapture::Release() {
    if (state_.load() == CaptureState::kCapturing) {
        StopCapture();
    }

    state_.store(CaptureState::kIdle);
    observer_.reset();
    return ErrorCode::kOk;
}

CaptureState FakeVideoCapture::GetState() const {
    return state_.load();
}

bool FakeVideoCapture::IsCapturing() const {
    return state_.load() == CaptureState::kCapturing;
}

ErrorCode FakeVideoCapture::GetStats(VideoCaptureStats* stats) const {
    if (!stats) return ErrorCode::kInvalidParam;

    std::lock_guard<std::mutex> lock(stats_mutex_);
    *stats = stats_;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoCapture::ResetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = VideoCaptureStats();
    seq_num_.store(0);
    timestamp_rtp_.store(0);
    return ErrorCode::kOk;
}

ErrorCode FakeVideoCapture::SetResolution(int width, int height) {
    if (width <= 0 || height <= 0) return ErrorCode::kInvalidParam;
    param_.width = width;
    param_.height = height;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoCapture::SetFrameRate(int fps) {
    if (fps <= 0) return ErrorCode::kInvalidParam;
    param_.target_fps = fps;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoCapture::SetRotation(int32_t rotation) {
    param_.rotation = rotation;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoCapture::SetMirror(bool mirror) {
    param_.mirror = mirror;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoCapture::SetBrightness(int value) {
    (void)value;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoCapture::SetContrast(int value) {
    (void)value;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoCapture::SetSaturation(int value) {
    (void)value;
    return ErrorCode::kOk;
}

void FakeVideoCapture::GenerateAndPushFrame() {
    auto observer = observer_.lock();
    if (!observer || !frame_generator_) return;

    VideoFrame frame;
    frame_generator_->GenerateFrame(&frame);

    frame.timestamp_us = duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();
    frame.timestamp_rtp = timestamp_rtp_.fetch_add(3000);
    frame.seq_num = seq_num_.fetch_add(1);
    frame.keyframe = true;

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_frames_captured++;
        stats_.total_bytes_captured += frame.GetBufferSize();
    }

    observer->OnFrameCaptured(frame);
}

void FakeVideoCapture::CaptureThreadLoop() {
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    int64_t frame_interval_us = 1000000LL / param_.target_fps;

    while (capturing_.load()) {
        auto frame_start = steady_clock::now();

        if (dist(gen) > drop_frame_rate_) {
            GenerateAndPushFrame();
        } else {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_frames_dropped++;
        }

        if (capture_latency_us_ > 0) {
            std::this_thread::sleep_for(microseconds(capture_latency_us_));
        }

        auto frame_end = steady_clock::now();
        auto elapsed = duration_cast<microseconds>(frame_end - frame_start).count();
        if (elapsed < frame_interval_us) {
            std::this_thread::sleep_for(microseconds(frame_interval_us - elapsed));
        }
    }
}

}  // namespace fake
}  // namespace minirtc
