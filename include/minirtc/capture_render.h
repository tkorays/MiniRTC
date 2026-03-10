/**
 * @file capture_render.h
 * @brief MiniRTC capture and render interfaces
 */

#ifndef MINIRTC_CAPTURE_RENDER_H
#define MINIRTC_CAPTURE_RENDER_H

#include "capture_render_types.h"
#include <memory>
#include <functional>

namespace minirtc {

// ============================================================================
// Video Capture Interfaces
// ============================================================================

class VideoCaptureObserver {
public:
    virtual ~VideoCaptureObserver() = default;
    virtual void OnFrameCaptured(const VideoFrame& frame) = 0;
    virtual void OnCaptureError(ErrorCode error_code, const std::string& error_msg) = 0;
    virtual void OnDeviceChanged() = 0;
};

class IVideoCapture {
public:
    virtual ~IVideoCapture() = default;

    virtual ErrorCode GetDevices(std::vector<VideoDeviceInfo>* devices) = 0;
    virtual ErrorCode SetDevice(const std::string& device_id) = 0;
    virtual std::string GetCurrentDevice() const = 0;

    virtual ErrorCode SetParam(const VideoCaptureParam& param) = 0;
    virtual ErrorCode GetParam(VideoCaptureParam* param) const = 0;

    virtual ErrorCode Initialize() = 0;
    virtual ErrorCode StartCapture(VideoCaptureObserver* observer) = 0;
    virtual ErrorCode StopCapture() = 0;
    virtual ErrorCode Release() = 0;

    virtual CaptureState GetState() const = 0;
    virtual bool IsCapturing() const = 0;

    virtual ErrorCode GetStats(VideoCaptureStats* stats) const = 0;
    virtual ErrorCode ResetStats() = 0;

    virtual ErrorCode SetResolution(int width, int height) = 0;
    virtual ErrorCode SetFrameRate(int fps) = 0;
    virtual ErrorCode SetRotation(int32_t rotation) = 0;
    virtual ErrorCode SetMirror(bool mirror) = 0;
    virtual ErrorCode SetBrightness(int value) = 0;
    virtual ErrorCode SetContrast(int value) = 0;
    virtual ErrorCode SetSaturation(int value) = 0;

    // For creating instances
    static std::unique_ptr<IVideoCapture> Create();
};

// ============================================================================
// Audio Capture Interfaces
// ============================================================================

class AudioCaptureObserver {
public:
    virtual ~AudioCaptureObserver() = default;
    virtual void OnFrameCaptured(const AudioFrame& frame) = 0;
    virtual void OnVolumeChanged(float volume_db) = 0;
    virtual void OnMuteDetected(bool is_muted) = 0;
    virtual void OnCaptureError(ErrorCode error_code, const std::string& error_msg) = 0;
    virtual void OnDeviceChanged() = 0;
};

class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;

    virtual ErrorCode GetDevices(std::vector<AudioDeviceInfo>* devices) = 0;
    virtual ErrorCode SetDevice(const std::string& device_id) = 0;
    virtual std::string GetCurrentDevice() const = 0;

    virtual ErrorCode SetParam(const AudioCaptureParam& param) = 0;
    virtual ErrorCode GetParam(AudioCaptureParam* param) const = 0;

    virtual ErrorCode Initialize() = 0;
    virtual ErrorCode StartCapture(AudioCaptureObserver* observer) = 0;
    virtual ErrorCode StopCapture() = 0;
    virtual ErrorCode Release() = 0;

    virtual CaptureState GetState() const = 0;
    virtual bool IsCapturing() const = 0;

    virtual ErrorCode GetStats(AudioCaptureStats* stats) const = 0;
    virtual ErrorCode ResetStats() = 0;

    virtual ErrorCode SetVolume(float volume) = 0;
    virtual ErrorCode GetVolume(float* volume) const = 0;
    virtual ErrorCode SetMute(bool mute) = 0;
    virtual ErrorCode GetMute(bool* mute) const = 0;

    virtual ErrorCode SetEchoReference(std::shared_ptr<AudioFrame> frame) = 0;

    static std::unique_ptr<IAudioCapture> Create();
};

// ============================================================================
// Video Renderer Interfaces
// ============================================================================

class VideoRenderObserver {
public:
    virtual ~VideoRenderObserver() = default;
    virtual void OnFrameRendered(const VideoFrame& frame, int64_t render_time_us) = 0;
    virtual void OnRenderError(ErrorCode error_code, const std::string& error_msg) = 0;
    virtual void OnRenderStats(const VideoRenderStats& stats) = 0;
};

class IVideoRenderer {
public:
    virtual ~IVideoRenderer() = default;

    virtual ErrorCode Initialize(const VideoRenderParam& param) = 0;
    virtual ErrorCode StartRender(VideoRenderObserver* observer) = 0;
    virtual ErrorCode StopRender() = 0;
    virtual ErrorCode Release() = 0;

    virtual PlayState GetState() const = 0;
    virtual bool IsRendering() const = 0;

    virtual ErrorCode RenderFrame(const VideoFrame& frame) = 0;
    virtual ErrorCode RenderFrameAsync(const VideoFrame& frame) = 0;

    virtual ErrorCode SetParam(const VideoRenderParam& param) = 0;
    virtual ErrorCode GetParam(VideoRenderParam* param) const = 0;

    virtual ErrorCode SetVisible(bool visible) = 0;
    virtual ErrorCode SetBackground(uint32_t rgba_color) = 0;
    virtual ErrorCode SetRotation(int32_t rotation) = 0;
    virtual ErrorCode SetMirror(bool mirror) = 0;
    virtual ErrorCode SetAlpha(float alpha) = 0;
    virtual ErrorCode SetDisplayRegion(int x, int y, int width, int height) = 0;

    virtual ErrorCode GetStats(VideoRenderStats* stats) const = 0;
    virtual ErrorCode ResetStats() = 0;

    virtual ErrorCode CreateWindow(const std::string& title, int width, int height) = 0;
    virtual ErrorCode DestroyWindow() = 0;
    virtual ErrorCode SetFullscreen(bool fullscreen) = 0;

    static std::unique_ptr<IVideoRenderer> Create();
};

// ============================================================================
// Audio Player Interfaces
// ============================================================================

class AudioPlayObserver {
public:
    virtual ~AudioPlayObserver() = default;
    virtual void OnFramePlayed(const AudioFrame& frame, int64_t play_time_us) = 0;
    virtual void OnPlayError(ErrorCode error_code, const std::string& error_msg) = 0;
    virtual void OnPlayStats(const AudioPlayStats& stats) = 0;
    virtual void OnBufferingChanged(bool is_buffering) = 0;
    virtual void OnMuteChanged(bool is_muted) = 0;
};

class IAudioPlayer {
public:
    virtual ~IAudioPlayer() = default;

    virtual ErrorCode GetDevices(std::vector<AudioDeviceInfo>* devices) = 0;
    virtual ErrorCode SetDevice(const std::string& device_id) = 0;
    virtual std::string GetCurrentDevice() const = 0;

    virtual ErrorCode SetParam(const AudioPlayParam& param) = 0;
    virtual ErrorCode GetParam(AudioPlayParam* param) const = 0;

    virtual ErrorCode Initialize() = 0;
    virtual ErrorCode StartPlay(AudioPlayObserver* observer) = 0;
    virtual ErrorCode StopPlay() = 0;
    virtual ErrorCode Release() = 0;

    virtual PlayState GetState() const = 0;
    virtual bool IsPlaying() const = 0;
    virtual bool IsBuffering() const = 0;

    virtual ErrorCode PlayFrame(const AudioFrame& frame) = 0;
    virtual ErrorCode PlayFrameList(const std::vector<AudioFrame>& frames) = 0;

    virtual ErrorCode SetVolume(float volume) = 0;
    virtual ErrorCode GetVolume(float* volume) const = 0;
    virtual ErrorCode SetMute(bool mute) = 0;
    virtual ErrorCode GetMute(bool* mute) const = 0;
    virtual ErrorCode SetPlaybackRate(float rate) = 0;
    virtual ErrorCode GetPlaybackRate(float* rate) const = 0;

    virtual ErrorCode SetBuffering(bool enable) = 0;
    virtual ErrorCode FlushBuffer() = 0;
    virtual ErrorCode GetBufferStatus(int* buffered_ms, int* queued_frames) const = 0;

    virtual ErrorCode GetStats(AudioPlayStats* stats) const = 0;
    virtual ErrorCode ResetStats() = 0;

    static std::unique_ptr<IAudioPlayer> Create();
};

// ============================================================================
// Factory Classes
// ============================================================================

class CaptureFactory {
public:
    enum class CaptureType {
        kPlatform,
        kFake,
    };

    static std::unique_ptr<IVideoCapture> CreateVideoCapture(CaptureType type = CaptureType::kFake);
    static std::unique_ptr<IAudioCapture> CreateAudioCapture(CaptureType type = CaptureType::kFake);

    // Register custom capture implementation
    using VideoCaptureCreator = std::function<std::unique_ptr<IVideoCapture>()>;
    using AudioCaptureCreator = std::function<std::unique_ptr<IAudioCapture>()>;

    static void RegisterVideoCapture(CaptureType type, VideoCaptureCreator creator);
    static void RegisterAudioCapture(CaptureType type, AudioCaptureCreator creator);

private:
    CaptureFactory() = default;
};

class RendererFactory {
public:
    enum class RendererType {
        kPlatform,
        kFake,
    };

    static std::unique_ptr<IVideoRenderer> CreateVideoRenderer(RendererType type = RendererType::kFake);
    static std::unique_ptr<IAudioPlayer> CreateAudioPlayer(RendererType type = RendererType::kFake);

    // Register custom renderer implementation
    using VideoRendererCreator = std::function<std::unique_ptr<IVideoRenderer>()>;
    using AudioPlayerCreator = std::function<std::unique_ptr<IAudioPlayer>()>;

    static void RegisterVideoRenderer(RendererType type, VideoRendererCreator creator);
    static void RegisterAudioPlayer(RendererType type, AudioPlayerCreator creator);

private:
    RendererFactory() = default;
};

}  // namespace minirtc

#endif  // MINIRTC_CAPTURE_RENDER_H
