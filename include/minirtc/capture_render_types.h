/**
 * @file capture_render_types.h
 * @brief MiniRTC capture and render types definition
 */

#ifndef MINIRTC_CAPTURE_RENDER_TYPES_H
#define MINIRTC_CAPTURE_RENDER_TYPES_H

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <cstdint>

namespace minirtc {

// ============================================================================
// Error Codes
// ============================================================================

enum class ErrorCode {
    kOk = 0,
    kNotInitialized = 1,
    kAlreadyStarted = 2,
    kNotStarted = 3,
    kInvalidParam = 4,
    kDeviceNotFound = 5,
    kDeviceBusy = 6,
    kPermissionDenied = 7,
    kResourceExhausted = 8,
    kOperationFailed = 9,
    kTimeout = 10,
    kUnsupported = 11,
};

// ============================================================================
// State Definitions
// ============================================================================

enum class CaptureState {
    kIdle = 0,
    kInitializing = 1,
    kReady = 2,
    kCapturing = 3,
    kStopping = 4,
    kError = 5,
};

enum class PlayState {
    kIdle = 0,
    kInitializing = 1,
    kReady = 2,
    kPlaying = 3,
    kPaused = 4,
    kStopping = 5,
    kError = 6,
};

// ============================================================================
// Video Types
// ============================================================================

enum class VideoPixelFormat {
    kI420 = 0,
    kNV12 = 1,
    kNV21 = 2,
    kRGBA = 3,
    kBGRA = 4,
    kRGB24 = 5,
    kMJPEG = 6,
};

enum class VideoColorSpace {
    kBT601 = 0,
    kBT709 = 1,
    kBT2020 = 2,
};

struct VideoFrame {
    VideoPixelFormat format = VideoPixelFormat::kI420;
    int width = 0;
    int height = 0;
    int stride_y = 0;
    int stride_u = 0;
    int stride_v = 0;
    int64_t timestamp_us = 0;
    uint32_t timestamp_rtp = 0;
    uint16_t seq_num = 0;
    bool keyframe = false;

    uint8_t* data_y = nullptr;
    uint8_t* data_u = nullptr;
    uint8_t* data_v = nullptr;

    std::vector<uint8_t> internal_buffer;
    bool is_external = false;

    VideoFrame() = default;

    VideoFrame(int w, int h, VideoPixelFormat fmt) 
        : format(fmt), width(w), height(h) {
        AllocateBuffer();
    }

    void AllocateBuffer() {
        size_t size = GetBufferSize();
        if (size > 0) {
            internal_buffer.resize(size);
            data_y = internal_buffer.data();
            SetStride();
        }
    }

    void SetStride() {
        stride_y = width;
        stride_u = (format == VideoPixelFormat::kNV12 || 
                    format == VideoPixelFormat::kNV21) ? width : width / 2;
        stride_v = stride_u;
    }

    size_t GetBufferSize() const;

    VideoFrame Clone() const;

    ~VideoFrame() {
        if (!is_external) {
            internal_buffer.clear();
        }
    }
};

struct VideoDeviceInfo {
    std::string device_id;
    std::string device_name;
    std::string unique_id;
    bool is_default = false;
    int32_t capabilities = 0;

    static constexpr int32_t kCapResolutionMask = 0xFFFF;
    static constexpr int32_t kCapFixedFramerate = 1 << 16;
    static constexpr int32_t kCapManualExposure = 1 << 17;
    static constexpr int32_t kCapAutoFocus = 1 << 18;
};

struct VideoCaptureParam {
    std::string device_id;
    int width = 1280;
    int height = 720;
    int target_fps = 30;
    int min_fps = 15;
    VideoPixelFormat format = VideoPixelFormat::kNV12;
    VideoColorSpace color_space = VideoColorSpace::kBT709;

    bool enable_hw_acceleration = true;
    bool enable_drop_late_frames = true;
    int32_t rotation = 0;
    bool mirror = false;

    bool auto_exposure = true;
    int exposure = -1;
    bool auto_white_balance = true;
    int white_balance = -1;
    bool auto_focus = true;
    int focus = -1;

    bool IsValid() const;
};

struct VideoCaptureStats {
    int64_t capture_start_time_us = 0;
    int64_t total_frames_captured = 0;
    int64_t total_frames_dropped = 0;
    int64_t total_bytes_captured = 0;
    int current_fps = 0;
    int actual_fps = 0;
    int dropped_fps = 0;
    int capture_width = 0;
    int capture_height = 0;
    double avg_capture_latency_ms = 0.0;
};

// ============================================================================
// Audio Types
// ============================================================================

enum class AudioSampleFormat {
    kInt16 = 0,
    kInt32 = 1,
    kFloat32 = 2,
};

enum class AudioChannelLayout {
    kMono = 1,
    kStereo = 2,
    k5_1 = 6,
    k7_1 = 8,
};

struct AudioFrame {
    AudioSampleFormat format = AudioSampleFormat::kInt16;
    int sample_rate = 48000;
    int channels = 1;
    AudioChannelLayout channel_layout = AudioChannelLayout::kMono;
    int samples_per_channel = 0;
    int64_t timestamp_us = 0;
    uint32_t timestamp_rtp = 0;
    uint16_t seq_num = 0;

    std::vector<uint8_t> data;

    AudioFrame() = default;

    AudioFrame(int rate, int ch, int samples) 
        : sample_rate(rate), channels(ch), samples_per_channel(samples) {
        AllocateBuffer();
    }

    void AllocateBuffer() {
        size_t size = GetDataSize();
        if (size > 0) {
            data.resize(size, 0);
        }
    }

    int GetSampleCount() const { return samples_per_channel; }
    size_t GetDataSize() const;
    double GetDurationMs() const;
};

struct AudioDeviceInfo {
    std::string device_id;
    std::string device_name;
    std::string unique_id;
    bool is_default = false;
    bool is_input = false;
    int32_t sample_rates = 0;
    int32_t channel_counts = 0;

    bool SupportsSampleRate(int rate) const {
        return (sample_rates & (1 << (rate / 1000 - 1))) != 0;
    }
};

struct AudioCaptureParam {
    std::string device_id;
    int sample_rate = 48000;
    int channels = 1;
    AudioChannelLayout channel_layout = AudioChannelLayout::kMono;
    AudioSampleFormat format = AudioSampleFormat::kInt16;
    int frames_per_buffer = 480;
    bool enable_vad = true;
    bool enable_agc = false;
    bool enable_noise_suppression = false;
    std::shared_ptr<AudioFrame> echo_reference_;

    bool IsValid() const;

    static int GetDefaultBufferSize(int sample_rate) {
        return sample_rate / 100;
    }
};

struct AudioCaptureStats {
    int64_t capture_start_time_us = 0;
    int64_t total_frames_captured = 0;
    int64_t total_bytes_captured = 0;
    int current_sample_rate = 0;
    int current_channels = 0;
    float current_volume_db = 0.0f;
    float avg_latency_ms = 0.0f;
    int clip_count = 0;
    int vad_active_count = 0;
};

// ============================================================================
// Render Types
// ============================================================================

enum class RenderWindowType {
    kUnknown = 0,
    kWindowHandle = 1,
    kMetalLayer = 2,
    kEGLSurface = 3,
    kD3DTexture = 4,
    kCVPixelBuffer = 5,
};

struct VideoRenderParam {
    RenderWindowType window_type = RenderWindowType::kUnknown;
    void* window_handle = nullptr;

    int output_width = 1280;
    int output_height = 720;
    VideoPixelFormat output_format = VideoPixelFormat::kBGRA;

    int display_width = 0;
    int display_height = 0;
    int32_t rotation = 0;
    bool mirror = false;

    bool enable_hw_acceleration = true;
    bool enable_vsync = true;
    float render_queue_threshold_ms = 50.0f;

    uint32_t background_color = 0xFF000000;

    bool enable_crop = false;
    float crop_left = 0.0f;
    float crop_top = 0.0f;
    float crop_right = 1.0f;
    float crop_bottom = 1.0f;

    enum class ScaleMode {
        kFit = 0,
        kFill = 1,
        kStretch = 2,
    };
    ScaleMode scale_mode = ScaleMode::kFit;

    bool IsValid() const;
};

struct VideoRenderStats {
    int64_t render_start_time_us = 0;
    int64_t total_frames_rendered = 0;
    int64_t total_frames_dropped = 0;
    int current_fps = 0;
    int render_width = 0;
    int render_height = 0;
    double avg_render_latency_ms = 0.0;
    double avg_frame_interval_ms = 0.0;
    int render_queue_size = 0;
    double buffer_latency_ms = 0.0;
};

struct AudioPlayParam {
    std::string device_id;
    int sample_rate = 48000;
    int channels = 2;
    AudioChannelLayout channel_layout = AudioChannelLayout::kStereo;
    AudioSampleFormat format = AudioSampleFormat::kInt16;

    int min_buffer_ms = 20;
    int max_buffer_ms = 200;
    int target_buffer_ms = 50;
    bool enable_adaptive_buffer = true;

    float volume = 1.0f;
    bool mute = false;
    float playback_rate = 1.0f;

    bool enable_hw_acceleration = true;
    bool enable_agc = false;
    bool enable_limiter = true;

    bool enable_spatial_audio = false;
    float pan = 0.0f;

    bool IsValid() const;
};

struct AudioPlayStats {
    int64_t play_start_time_us = 0;
    int64_t total_frames_played = 0;
    int64_t total_bytes_played = 0;
    int current_sample_rate = 0;
    int current_channels = 0;
    float current_volume = 0.0f;
    int buffer_ms = 0;
    int underrun_count = 0;
    int overrun_count = 0;
    double avg_latency_ms = 0.0;
    double avg_playback_latency_ms = 0.0;
};

// ============================================================================
// Inline implementations
// ============================================================================

inline size_t VideoFrame::GetBufferSize() const {
    switch (format) {
        case VideoPixelFormat::kI420:
            return static_cast<size_t>(width * height * 3 / 2);
        case VideoPixelFormat::kNV12:
        case VideoPixelFormat::kNV21:
            return static_cast<size_t>(width * height * 3 / 2);
        case VideoPixelFormat::kRGBA:
        case VideoPixelFormat::kBGRA:
            return static_cast<size_t>(width * height * 4);
        case VideoPixelFormat::kRGB24:
            return static_cast<size_t>(width * height * 3);
        default:
            return 0;
    }
}

inline VideoFrame VideoFrame::Clone() const {
    VideoFrame cloned;
    cloned.format = format;
    cloned.width = width;
    cloned.height = height;
    cloned.stride_y = stride_y;
    cloned.stride_u = stride_u;
    cloned.stride_v = stride_v;
    cloned.timestamp_us = timestamp_us;
    cloned.timestamp_rtp = timestamp_rtp;
    cloned.seq_num = seq_num;
    cloned.keyframe = keyframe;
    cloned.AllocateBuffer();
    if (!internal_buffer.empty()) {
        cloned.internal_buffer = internal_buffer;
        cloned.data_y = cloned.internal_buffer.data();
        cloned.SetStride();
    }
    return cloned;
}

inline size_t AudioFrame::GetDataSize() const {
    size_t bytes_per_sample = 2;
    switch (format) {
        case AudioSampleFormat::kInt16: bytes_per_sample = 2; break;
        case AudioSampleFormat::kInt32: bytes_per_sample = 4; break;
        case AudioSampleFormat::kFloat32: bytes_per_sample = 4; break;
    }
    return static_cast<size_t>(samples_per_channel * channels * bytes_per_sample);
}

inline double AudioFrame::GetDurationMs() const {
    if (sample_rate <= 0) return 0.0;
    return static_cast<double>(samples_per_channel) * 1000.0 / sample_rate;
}

inline bool VideoCaptureParam::IsValid() const {
    return width > 0 && height > 0 && 
           target_fps > 0 && min_fps > 0 &&
           min_fps <= target_fps;
}

inline bool AudioCaptureParam::IsValid() const {
    return sample_rate > 0 && channels > 0 && frames_per_buffer > 0;
}

inline bool VideoRenderParam::IsValid() const {
    return output_width > 0 && output_height > 0;
}

inline bool AudioPlayParam::IsValid() const {
    return sample_rate > 0 && channels > 0 &&
           volume >= 0.0f && volume <= 1.0f &&
           playback_rate >= 0.5f && playback_rate <= 2.0f;
}

}  // namespace minirtc

#endif  // MINIRTC_CAPTURE_RENDER_TYPES_H
