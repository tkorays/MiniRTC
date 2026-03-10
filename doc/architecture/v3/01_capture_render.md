# MiniRTC 采集/播放模块接口设计文档 v3.0

**版本**: 3.0
**日期**: 2026-03-10
**状态**: 初稿设计

---

## 1. 概述

本文档描述MiniRTC v3.0中音视频采集与播放模块的详细接口设计。基于v2架构，本版本将采集/播放接口进一步细化，增加了生命周期管理、配置管理、状态机、事件回调等机制，并提供Fake实现用于单元测试。

### 1.1 设计目标

- **平台抽象**: 统一跨平台采集/播放接口
- **可测试性**: 提供Fake实现，支持纯内存测试
- **可扩展性**: 工厂模式支持多后端实现
- **状态清晰**: 明确的状态机和错误处理
- **性能可控**: 支持帧率/码率控制

### 1.2 模块架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         采集/播放模块架构                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                     Capture/Render Factory                     │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐            │   │
│  │  │  Platform   │  │    Fake     │  │   Custom    │            │   │
│  │  │  Factory    │  │   Factory   │  │  Factory    │            │   │
│  │  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘            │   │
│  └─────────┼───────────────┼───────────────┼─────────────────────┘   │
│            │               │               │                          │
│  ┌─────────▼───────────────▼───────────────▼─────────────────────┐   │
│  │                         接口层                                  │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐            │   │
│  │  │IVideoCapture│  │IAudioCapture│  │IVideoRenderer│            │   │
│  │  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘            │   │
│               │               │                      │   │
│  │  ┌──────▼──────  │         │┐  ┌──────▼──────┐  ┌──────▼──────┐            │   │
│  │  │VideoCapture │  │AudioCapture │  │VideoRenderer │            │   │
│  │  │ (Platform)  │  │ (Platform)  │  │ (Platform)   │            │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘            │   │
│  │         │               │               │                      │   │
│  │  ┌──────▼──────┐  ┌──────▼──────┐  ┌──────▼──────┐            │   │
│  │  │FakeVideoCap │  │FakeAudioCap │  │FakeVideoRdr │            │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘            │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 基础类型定义

### 2.1 公共错误码

```cpp
namespace minirtc {

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

}  // namespace minirtc
```

### 2.2 视频类型

```cpp
namespace minirtc {

// 像素格式
enum class VideoPixelFormat {
  kI420 = 0,    // YUV 420 Planar
  kNV12 = 1,    // YUV 420 Semi-Planar
  kNV21 = 2,    // YUV 420 Semi-Planar (UV顺序反转)
  kRGBA = 3,    // RGBA8888
  kBGRA = 4,    // BGRA8888
  kRGB24 = 5,   // RGB888
  kMJPEG = 6,   // Motion JPEG
};

// 色彩空间
enum class VideoColorSpace {
  kBT601 = 0,   // ITU-R BT.601
  kBT709 = 1,   // ITU-R BT.709
  kBT2020 = 2,  // ITU-R BT.2020
};

// 视频帧
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
};

// 视频设备信息
struct VideoDeviceInfo {
  std::string device_id;
  std::string device_name;
  std::string unique_id;
  bool is_default = false;
  int32_t capabilities = 0;
};

}  // namespace minirtc
```

### 2.3 音频类型

```cpp
namespace minirtc {

enum class AudioSampleFormat {
  kInt16 = 0, kInt32 = 1, kFloat32 = 2,
};

enum class AudioChannelLayout {
  kMono = 1, kStereo = 2, k5_1 = 6, k7_1 = 8,
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
};

struct AudioDeviceInfo {
  std::string device_id;
  std::string device_name;
  std::string unique_id;
  bool is_default = false;
  bool is_input = false;
  int32_t sample_rates = 0;
  int32_t channel_counts = 0;
};

}  // namespace minirtc
```

---

## 3. IVideoCapture 详细接口设计

### 3.1 接口定义

```cpp
namespace minirtc {

class VideoCaptureObserver {
 public:
  virtual ~VideoCaptureObserver() = default;
  virtual void OnFrameCaptured(const VideoFrame& frame) = 0;
  virtual void OnCaptureError(ErrorCode error_code, const std::string& error_msg) = 0;
  virtual void OnDeviceChanged() = 0;
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

class IVideoCapture : public IRefCounted {
 public:
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
};

}  // namespace minirtc
```

### 3.2 状态机

```
Idle -> Initializing -> Ready -> Capturing -> Stopping -> Idle
     Any State -> Error -> Idle (via Release)
```

---

## 4. IAudioCapture 详细接口设计

```cpp
namespace minirtc {

class AudioCaptureObserver {
 public:
  virtual ~AudioCaptureObserver() = default;
  virtual void OnFrameCaptured(const AudioFrame& frame) = 0;
  virtual void OnVolumeChanged(float volume_db) = 0;
  virtual void OnMuteDetected(bool is_muted) = 0;
  virtual void OnCaptureError(ErrorCode error_code, const std::string& error_msg) = 0;
  virtual void OnDeviceChanged() = 0;
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

class IAudioCapture : public IRefCounted {
 public:
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
};

}  // namespace minirtc
```

---

## 5. IVideoRenderer 详细接口设计

```cpp
namespace minirtc {

class VideoRenderObserver {
 public:
  virtual ~VideoRenderObserver() = default;
  virtual void OnFrameRendered(const VideoFrame& frame, int64_t render_time_us) = 0;
  virtual void OnRenderError(ErrorCode error_code, const std::string& error_msg) = 0;
  virtual void OnRenderStats(const VideoRenderStats& stats) = 0;
};

enum class RenderWindowType {
  kUnknown = 0, kWindowHandle = 1, kMetalLayer = 2,
  kEGLSurface = 3, kD3DTexture = 4, kCVPixelBuffer = 5,
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
  enum class ScaleMode { kFit = 0, kFill = 1, kStretch = 2 };
  ScaleMode scale_mode = ScaleMode::kFit;
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

class IVideoRenderer : public IRefCounted {
 public:
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
};

}  // namespace minirtc
```

---

## 6. IAudioPlayer 详细接口设计

```cpp
namespace minirtc {

class AudioPlayObserver {
 public:
  virtual ~AudioPlayObserver() = default;
  virtual void OnFramePlayed(const AudioFrame& frame, int64_t play_time_us) = 0;
  virtual void OnPlayError(ErrorCode error_code, const std::string& error_msg) = 0;
  virtual void OnPlayStats(const AudioPlayStats& stats) = 0;
  virtual void OnBufferingChanged(bool is_buffering) = 0;
  virtual void OnMuteChanged(bool is_muted) = 0;
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

class IAudioPlayer : public IRefCounted {
 public:
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
};

}  // namespace minirtc
```

---

## 7. Fake 实现设计

### 7.1 FakeVideoCapture

```cpp
namespace minirtc {
namespace fake {

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
  int width_, height_;
  uint32_t rgba_color_;
};

class GradientGenerator : public VideoFrameGenerator {
 public:
  GradientGenerator(int width, int height);
  void GenerateFrame(VideoFrame* frame) override;
 private:
  int width_, height_;
};

class FakeVideoCapture : public IVideoCapture {
 public:
  explicit FakeVideoCapture();
  void SetFrameGenerator(std::unique_ptr<VideoFrameGenerator> generator);
  void SetCaptureLatency(int64_t latency_us);
  void SetDropFrameRate(float rate);
  
  ErrorCode GetDevices(std::vector<VideoDeviceInfo>* devices) override;
  ErrorCode SetDevice(const std::string& device_id) override;
  ErrorCode SetParam(const VideoCaptureParam& param) override;
  ErrorCode Initialize() override;
  ErrorCode StartCapture(VideoCaptureObserver* observer) override;
  ErrorCode StopCapture() override;
  ErrorCode Release() override;
  CaptureState GetState() const override;
  bool IsCapturing() const override;
  ErrorCode GetStats(VideoCaptureStats* stats) const override;
  
 private:
  void GenerateAndPushFrame();
  VideoCaptureObserver* observer_ = nullptr;
  std::unique_ptr<VideoFrameGenerator> frame_generator_;
  int64_t capture_latency_us_ = 0;
  float drop_frame_rate_ = 0.0f;
  std::atomic<bool> capturing_{false};
  std::thread capture_thread_;
};

}  // namespace fake
}  // namespace minirtc
```

### 7.2 FakeAudioCapture

```cpp
namespace minirtc {
namespace fake {

class AudioFrameGenerator {
 public:
  virtual ~AudioFrameGenerator() = default;
  virtual void GenerateFrame(AudioFrame* frame) = 0;
};

class SineWaveGenerator : public AudioFrameGenerator {
 public:
  SineWaveGenerator(int sample_rate, int frequency_hz, int channels);
  void GenerateFrame(AudioFrame* frame) override;
 private:
  int sample_rate_, frequency_hz_, channels_;
  int64_t sample_index_ = 0;
};

class SilenceGenerator : public AudioFrameGenerator {
 public:
  SilenceGenerator(int sample_rate, int channels);
  void GenerateFrame(AudioFrame* frame) override;
};

class FakeAudioCapture : public IAudioCapture {
 public:
  explicit FakeAudioCapture();
  void SetFrameGenerator(std::unique_ptr<AudioFrameGenerator> generator);
  void SetGenerateSineWave(int frequency_hz);
  void SetSilence();
  
  ErrorCode GetDevices(std::vector<AudioDeviceInfo>* devices) override;
  ErrorCode SetDevice(const std::string& device_id) override;
  ErrorCode SetParam(const AudioCaptureParam& param) override;
  ErrorCode Initialize() override;
  ErrorCode StartCapture(AudioCaptureObserver* observer) override;
  ErrorCode StopCapture() override;
  ErrorCode Release() override;
  CaptureState GetState() const override;
  bool IsCapturing() const override;
  ErrorCode GetStats(AudioCaptureStats* stats) const override;
  ErrorCode SetVolume(float volume) override;
  ErrorCode GetVolume(float* volume) const override;
  ErrorCode SetMute(bool mute) override;
  ErrorCode GetMute(bool* mute) const override;
  
 private:
  AudioCaptureObserver* observer_ = nullptr;
  std::unique_ptr<AudioFrameGenerator> frame_generator_;
  float volume_ = 1.0f;
  bool mute_ = false;
  std::atomic<bool> capturing_{false};
  std::thread capture_thread_;
};

}  // namespace fake
}  // namespace minirtc
```

### 7.3 FakeVideoRenderer

```cpp
namespace minirtc {
namespace fake {

class FakeVideoRenderer : public IVideoRenderer {
 public:
  explicit FakeVideoRenderer();
  void SetRenderLatency(int64_t latency_us);
  void SetDropFrameRate(float rate);
  
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
  
 private:
  VideoRenderObserver* observer_ = nullptr;
  int64_t render_latency_us_ = 0;
  float drop_frame_rate_ = 0.0f;
  VideoRenderStats stats_;
};

}  // namespace fake
}  // namespace minirtc
```

### 7.4 FakeAudioPlayer

```cpp
namespace minirtc {
namespace fake {

class FakeAudioPlayer : public IAudioPlayer {
 public:
  explicit FakeAudioPlayer();
  void SetBufferLatency(int64_t latency_ms);
  void SetUnderrunEnabled(bool enabled);
  
  ErrorCode GetDevices(std::vector<AudioDeviceInfo>* devices) override;
  ErrorCode SetDevice(const std::string& device_id) override;
  ErrorCode SetParam(const AudioPlayParam& param) override;
  ErrorCode Initialize() override;
  ErrorCode StartPlay(AudioPlayObserver* observer) override;
  ErrorCode StopPlay() override;
  ErrorCode Release() override;
  PlayState GetState() const override;
  bool IsPlaying() const override;
  bool IsBuffering() const override;
  ErrorCode PlayFrame(const AudioFrame& frame) override;
  ErrorCode PlayFrameList(const std::vector<AudioFrame>& frames) override;
  ErrorCode SetVolume(float volume) override;
  ErrorCode GetVolume(float* volume) const override;
  ErrorCode SetMute(bool mute) override;
  ErrorCode GetMute(bool* mute) const override;
  ErrorCode SetPlaybackRate(float rate) override;
  ErrorCode GetPlaybackRate(float* rate) const override;
  ErrorCode SetBuffering(bool enable) override;
  ErrorCode FlushBuffer() override;
  ErrorCode GetBufferStatus(int* buffered_ms, int* queued_frames) const override;
  ErrorCode GetStats(AudioPlayStats* stats) const override;
  ErrorCode ResetStats() override;
  
 private:
  AudioPlayObserver* observer_ = nullptr;
  int64_t buffer_latency_ms_ = 0;
  bool underrun_enabled_ = false;
  AudioPlayStats stats_;
};

}  // namespace fake
}  // namespace minirtc
```

---

## 8. 工厂模式设计

```cpp
namespace minirtc {

enum class CaptureRenderFactoryType {
  kPlatform = 0,   // 平台实现
  kFake = 1,       // Fake实现 (用于测试)
  kCustom = 2,     // 自定义实现
};

class CaptureRenderFactory {
 public:
  static std::unique_ptr<IVideoCapture> CreateVideoCapture(
      CaptureRenderFactoryType type);
  
  static std::unique_ptr<IAudioCapture> CreateAudioCapture(
      CaptureRenderFactoryType type);
  
  static std::unique_ptr<IVideoRenderer> CreateVideoRenderer(
      CaptureRenderFactoryType type);
  
  static std::unique_ptr<IAudioPlayer> CreateAudioPlayer(
      CaptureRenderFactoryType type);
  
  using VideoCaptureCreator = std::function<std::unique_ptr<IVideoCapture>()>;
  using AudioCaptureCreator = std::function<std::unique_ptr<IAudioCapture>()>;
  using VideoRendererCreator = std::function<std::unique_ptr<IVideoRenderer>()>;
  using AudioPlayerCreator = std::function<std::unique_ptr<IAudioPlayer>()>;
  
  static void RegisterVideoCapture(CaptureRenderFactoryType type, 
                                    VideoCaptureCreator creator);
  static void RegisterAudioCapture(CaptureRenderFactoryType type,
                                    AudioCaptureCreator creator);
  static void RegisterVideoRenderer(CaptureRenderFactoryType type,
                                     VideoRendererCreator creator);
  static void RegisterAudioPlayer(CaptureRenderFactoryType type,
                                   AudioPlayerCreator creator);
};
```

### 使用示例

```cpp
// 平台实现 (生产环境)
auto video_capture = CaptureRenderFactory::CreateVideoCapture(
    CaptureRenderFactory::kPlatform);
auto audio_player = CaptureRenderFactory::CreateAudioPlayer(
    CaptureRenderFactory::kPlatform);

// Fake实现 (测试环境)
auto fake_video_capture = CaptureRenderFactory::CreateVideoCapture(
    CaptureRenderFactory::kFake);
fake_video_capture->SetFrameGenerator(
    std::make_unique<SolidColorGenerator>(640, 480, 0xFF0000FF));

// 自定义实现
CaptureRenderFactory::RegisterVideoCapture(
    CaptureRenderFactory::kCustom,
    []() { return std::make_unique<MyCustomVideoCapture>(); });
```

---

## 9. 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v3.0 | 2026-03-10 | 采集/播放接口设计初稿 |
