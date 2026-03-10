/**
 * @file fake_video_renderer.cc
 * @brief Fake video renderer implementation
 */

#include "minirtc/fake_video_renderer.h"

#include <chrono>
#include <random>
#include <cmath>

namespace minirtc {
namespace fake {

using namespace std::chrono;

// ============================================================================
// FakeVideoRenderer
// ============================================================================

FakeVideoRenderer::FakeVideoRenderer() {
    display_width_ = param_.output_width;
    display_height_ = param_.output_height;
}

FakeVideoRenderer::~FakeVideoRenderer() {
    StopRender();
    Release();
}

void FakeVideoRenderer::SetRenderLatency(int64_t latency_us) {
    render_latency_us_ = latency_us;
}

void FakeVideoRenderer::SetDropFrameRate(float rate) {
    drop_frame_rate_ = std::max(0.0f, std::min(1.0f, rate));
}

void FakeVideoRenderer::SetProcessFrames(bool process) {
    process_frames_ = process;
}

ErrorCode FakeVideoRenderer::Initialize(const VideoRenderParam& param) {
    if (state_.load() != PlayState::kIdle) {
        return ErrorCode::kAlreadyStarted;
    }

    if (!param.IsValid()) {
        return ErrorCode::kInvalidParam;
    }

    param_ = param;
    display_width_ = param.output_width;
    display_height_ = param.output_height;

    state_.store(PlayState::kInitializing);
    std::this_thread::sleep_for(milliseconds(10));
    state_.store(PlayState::kReady);

    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::StartRender(VideoRenderObserver* observer) {
    if (state_.load() == PlayState::kPlaying) {
        return ErrorCode::kAlreadyStarted;
    }

    if (state_.load() != PlayState::kReady) {
        return ErrorCode::kNotInitialized;
    }

    observer_ = observer;
    rendering_.store(true);
    state_.store(PlayState::kPlaying);

    stats_.render_start_time_us = duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();

    render_thread_ = std::thread(&FakeVideoRenderer::RenderThreadLoop, this);

    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::StopRender() {
    if (state_.load() != PlayState::kPlaying) {
        return ErrorCode::kNotStarted;
    }

    state_.store(PlayState::kStopping);
    rendering_.store(false);
    frame_cv_.notify_all();

    if (render_thread_.joinable()) {
        render_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(frame_queue_mutex_);
        while (!frame_queue_.empty()) {
            frame_queue_.pop();
        }
    }

    state_.store(PlayState::kReady);
    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::Release() {
    if (state_.load() == PlayState::kPlaying) {
        StopRender();
    }

    state_.store(PlayState::kIdle);
    observer_ = nullptr;
    return ErrorCode::kOk;
}

PlayState FakeVideoRenderer::GetState() const {
    return state_.load();
}

bool FakeVideoRenderer::IsRendering() const {
    return state_.load() == PlayState::kPlaying;
}

ErrorCode FakeVideoRenderer::RenderFrame(const VideoFrame& frame) {
    if (state_.load() != PlayState::kPlaying) {
        return ErrorCode::kNotStarted;
    }

    ProcessFrame(frame);
    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::RenderFrameAsync(const VideoFrame& frame) {
    if (state_.load() != PlayState::kPlaying) {
        return ErrorCode::kNotStarted;
    }

    {
        std::lock_guard<std::mutex> lock(frame_queue_mutex_);
        if (frame_queue_.size() < 10) {
            frame_queue_.push(frame);
        }
    }
    frame_cv_.notify_one();

    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::SetParam(const VideoRenderParam& param) {
    if (!param.IsValid()) return ErrorCode::kInvalidParam;
    param_ = param;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::GetParam(VideoRenderParam* param) const {
    if (!param) return ErrorCode::kInvalidParam;
    *param = param_;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::SetVisible(bool visible) {
    visible_ = visible;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::SetBackground(uint32_t rgba_color) {
    background_color_ = rgba_color;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::SetRotation(int32_t rotation) {
    rotation_ = rotation;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::SetMirror(bool mirror) {
    mirror_ = mirror;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::SetAlpha(float alpha) {
    alpha_ = std::max(0.0f, std::min(1.0f, alpha));
    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::SetDisplayRegion(int x, int y, int width, int height) {
    display_x_ = x;
    display_y_ = y;
    display_width_ = width;
    display_height_ = height;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::GetStats(VideoRenderStats* stats) const {
    if (!stats) return ErrorCode::kInvalidParam;

    std::lock_guard<std::mutex> lock(stats_mutex_);
    *stats = stats_;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::ResetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = VideoRenderStats();
    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::CreateWindow(const std::string& title, int width, int height) {
    (void)title;
    (void)width;
    (void)height;
    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::DestroyWindow() {
    return ErrorCode::kOk;
}

ErrorCode FakeVideoRenderer::SetFullscreen(bool fullscreen) {
    (void)fullscreen;
    return ErrorCode::kOk;
}

void FakeVideoRenderer::ProcessFrame(const VideoFrame& frame) {
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    if (dist(gen) < drop_frame_rate_) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_frames_dropped++;
        return;
    }

    if (render_latency_us_ > 0 && process_frames_) {
        std::this_thread::sleep_for(microseconds(render_latency_us_));
    }

    int64_t render_time_us = duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_frames_rendered++;
        stats_.render_width = frame.width;
        stats_.render_height = frame.height;
    }

    if (observer_) {
        observer_->OnFrameRendered(frame, render_time_us);
    }
}

void FakeVideoRenderer::RenderThreadLoop() {
    while (rendering_.load()) {
        VideoFrame frame;

        {
            std::unique_lock<std::mutex> lock(frame_queue_mutex_);
            frame_cv_.wait_for(lock, milliseconds(100), [this] {
                return !frame_queue_.empty() || !rendering_.load();
            });

            if (!rendering_.load()) break;

            if (!frame_queue_.empty()) {
                frame = std::move(frame_queue_.front());
                frame_queue_.pop();
            }
        }

        if (frame.width > 0 && frame.height > 0) {
            ProcessFrame(frame);
        }
    }
}

}  // namespace fake
}  // namespace minirtc
