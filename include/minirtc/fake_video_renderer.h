/**
 * @file fake_video_renderer.h
 * @brief Fake video renderer implementation for testing
 */

#ifndef MINIRTC_FAKE_VIDEO_RENDERER_H
#define MINIRTC_FAKE_VIDEO_RENDERER_H

#include "capture_render.h"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace minirtc {
namespace fake {

// ============================================================================
// Fake Video Renderer
// ============================================================================

class FakeVideoRenderer : public IVideoRenderer {
public:
    FakeVideoRenderer();
    ~FakeVideoRenderer() override;

    // Configuration
    void SetRenderLatency(int64_t latency_us);
    void SetDropFrameRate(float rate);
    void SetProcessFrames(bool process);

    // IVideoRenderer interface
    ErrorCode Initialize(const VideoRenderParam& param) override;
    ErrorCode StartRender(VideoRenderObserver* observer) override;
    ErrorCode StopRender() override;
    ErrorCode Release() override;

    PlayState GetState() const override;
    bool IsRendering() const override;

    ErrorCode RenderFrame(const VideoFrame& frame) override;
    ErrorCode RenderFrameAsync(const VideoFrame& frame) override;

    ErrorCode SetParam(const VideoRenderParam& param) override;
    ErrorCode GetParam(VideoRenderParam* param) const override;

    ErrorCode SetVisible(bool visible) override;
    ErrorCode SetBackground(uint32_t rgba_color) override;
    ErrorCode SetRotation(int32_t rotation) override;
    ErrorCode SetMirror(bool mirror) override;
    ErrorCode SetAlpha(float alpha) override;
    ErrorCode SetDisplayRegion(int x, int y, int width, int height) override;

    ErrorCode GetStats(VideoRenderStats* stats) const override;
    ErrorCode ResetStats() override;

    ErrorCode CreateWindow(const std::string& title, int width, int height) override;
    ErrorCode DestroyWindow() override;
    ErrorCode SetFullscreen(bool fullscreen) override;

private:
    void ProcessFrame(const VideoFrame& frame);
    void RenderThreadLoop();

    VideoRenderObserver* observer_ = nullptr;
    VideoRenderParam param_;

    int64_t render_latency_us_ = 0;
    float drop_frame_rate_ = 0.0f;
    bool process_frames_ = true;

    bool visible_ = true;
    uint32_t background_color_ = 0xFF000000;
    int32_t rotation_ = 0;
    bool mirror_ = false;
    float alpha_ = 1.0f;
    int display_x_ = 0;
    int display_y_ = 0;
    int display_width_ = 0;
    int display_height_ = 0;

    std::atomic<PlayState> state_{PlayState::kIdle};
    std::atomic<bool> rendering_{false};
    std::thread render_thread_;

    mutable std::mutex stats_mutex_;
    VideoRenderStats stats_;

    mutable std::mutex frame_queue_mutex_;
    std::queue<VideoFrame> frame_queue_;
    std::condition_variable frame_cv_;
    std::atomic<bool> stop_processing_{false};
};

}  // namespace fake
}  // namespace minirtc

#endif  // MINIRTC_FAKE_VIDEO_RENDERER_H
