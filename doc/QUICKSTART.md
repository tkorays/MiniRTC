# MiniRTC 快速开始指南

本文档将帮助你快速上手 MiniRTC，通过简单的示例了解如何使用库的核心功能。

## 目录

1. [环境准备](#1-环境准备)
2. [最简单的示例：音视频采集与播放](#2-最简单的示例音视频采集与播放)
3. [完整通话流程示例](#3-完整通话流程示例)
4. [常见问题](#4-常见问题)

---

## 1. 环境准备

### 1.1 构建库

```bash
# 克隆项目
git clone https://github.com/your-repo/MiniRTC.git
cd MiniRTC

# 创建构建目录
mkdir build && cd build

# 配置并构建
cmake .. -DBUILD_TESTS=ON
cmake --build . -j$(nproc)
```

### 1.2 在你的项目中使用 MiniRTC

CMake 配置：

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyApp)

# 找到 MiniRTC
find_package(minirtc REQUIRED)

# 你的可执行文件
add_executable(my_app main.cpp)
target_link_libraries(my_app minirtc)
```

---

## 2. 最简单的示例：音视频采集与播放

### 2.1 视频采集示例

```cpp
#include <minirtc/minirtc.h>
#include <minirtc/capture_render.h>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace minirtc;

// 简单的视频采集观察者
class SimpleVideoObserver : public VideoCaptureObserver {
public:
    void OnFrameCaptured(const VideoFrame& frame) override {
        std::cout << "Captured frame: " 
                  << frame.width << "x" << frame.height 
                  << " at " << frame.timestamp_us << " us" 
                  << std::endl;
    }
    
    void OnCaptureError(ErrorCode error_code, 
                        const std::string& error_msg) override {
        std::cerr << "Capture error: " << error_msg << std::endl;
    }
    
    void OnDeviceChanged() override {
        std::cout << "Device changed" << std::endl;
    }
};

int main() {
    // 1. 初始化库
    if (minirtc_init() != MINIRTC_OK) {
        std::cerr << "Failed to initialize MiniRTC" << std::endl;
        return -1;
    }
    
    std::cout << "MiniRTC version: " << minirtc_version() << std::endl;
    
    // 2. 创建视频采集器（使用 Fake 设备，用于测试）
    auto video_capture = CaptureFactory::CreateVideoCapture(
        CaptureFactory::CaptureType::kFake
    );
    
    // 3. 设置采集参数
    VideoCaptureParam param;
    param.width = 1280;
    param.height = 720;
    param.fps = 30;
    video_capture->SetParam(param);
    
    // 4. 初始化
    if (video_capture->Initialize() != ErrorCode::kOk) {
        std::cerr << "Failed to initialize video capture" << std::endl;
        return -1;
    }
    
    // 5. 开始采集
    auto observer = std::make_shared<SimpleVideoObserver>();
    if (video_capture->StartCapture(observer) != ErrorCode::kOk) {
        std::cerr << "Failed to start capture" << std::endl;
        return -1;
    }
    
    std::cout << "Video capture started, press Enter to stop..." << std::endl;
    std::getchar();
    
    // 6. 停止采集
    video_capture->StopCapture();
    video_capture->Release();
    
    // 7. 关闭库
    minirtc_shutdown();
    
    return 0;
}
```

### 2.2 音频采集示例

```cpp
#include <minirtc/minirtc.h>
#include <minirtc/capture_render.h>
#include <iostream>

using namespace minirtc;

class SimpleAudioObserver : public AudioCaptureObserver {
public:
    void OnFrameCaptured(const AudioFrame& frame) override {
        std::cout << "Captured audio: " 
                  << frame.sample_rate << " Hz, "
                  << frame.channels << " ch, "
                  << frame.samples_per_channel << " samples"
                  << std::endl;
    }
    
    void OnVolumeChanged(float volume_db) override {
        std::cout << "Volume: " << volume_db << " dB" << std::endl;
    }
    
    void OnMuteDetected(bool is_muted) override {}
    void OnCaptureError(ErrorCode, const std::string&) override {}
    void OnDeviceChanged() override {}
};

int main() {
    minirtc_init();
    
    auto audio_capture = CaptureFactory::CreateAudioCapture(
        CaptureFactory::CaptureType::kFake
    );
    
    AudioCaptureParam param;
    param.sample_rate = 48000;
    param.channels = 2;
    audio_capture->SetParam(param);
    
    audio_capture->Initialize();
    audio_capture->StartCapture(new SimpleAudioObserver());
    
    std::cout << "Audio capture started, press Enter to stop..." << std::endl;
    std::getchar();
    
    audio_capture->StopCapture();
    audio_capture->Release();
    minirtc_shutdown();
    
    return 0;
}
```

### 2.3 视频渲染示例

```cpp
#include <minirtc/minirtc.h>
#include <minirtc/capture_render.h>
#include <iostream>

using namespace minirtc;

int main() {
    minirtc_init();
    
    // 创建视频渲染器
    auto renderer = RendererFactory::CreateVideoRenderer(
        RendererFactory::RendererType::kFake
    );
    
    VideoRenderParam param;
    param.width = 1280;
    param.height = 720;
    param.pixel_format = VideoPixelFormat::kRGBA;
    renderer->SetParam(param);
    
    renderer->Initialize();
    
    // 创建一个测试视频帧
    VideoFrame frame;
    frame.width = 1280;
    frame.height = 720;
    frame.format = VideoPixelFormat::kRGBA;
    frame.timestamp_us = 0;
    
    // 分配内存（RGBA: 1280 * 720 * 4 bytes）
    frame.data = new uint8_t[1280 * 720 * 4];
    frame.stride[0] = 1280 * 4;
    
    // 填充红色
    memset(frame.data, 255, 1280 * 720 * 4);  // Red
    
    // 渲染帧
    renderer->RenderFrame(frame);
    
    std::cout << "Frame rendered, press Enter to exit..." << std::endl;
    std::getchar();
    
    // 清理
    delete[] frame.data;
    renderer->Release();
    minirtc_shutdown();
    
    return 0;
}
```

---

## 3. 完整通话流程示例

这是一个模拟的端到端音视频通话流程，展示了如何将各模块组合在一起：

```cpp
#include <minirtc/minirtc.h>
#include <minirtc/capture_render.h>
#include <minirtc/codec/codec_factory.h>
#include <minirtc/codec/h264_encoder.h>
#include <minirtc/codec/h264_decoder.h>
#include <minirtc/codec/opus_encoder.h>
#include <minirtc/codec/opus_decoder.h>
#include <minirtc/jitter_buffer.h>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace minirtc;

// 视频采集观察者
class VideoCaptureCallback : public VideoCaptureObserver {
public:
    std::function<void(const VideoFrame&)> on_frame;
    
    void OnFrameCaptured(const VideoFrame& frame) override {
        if (on_frame) {
            on_frame(frame);
        }
    }
    void OnCaptureError(ErrorCode, const std::string&) override {}
    void OnDeviceChanged() override {}
};

// 音频采集观察者
class AudioCaptureCallback : public AudioCaptureObserver {
public:
    std::function<void(const AudioFrame&)> on_frame;
    
    void OnFrameCaptured(const AudioFrame& frame) override {
        if (on_frame) {
            on_frame(frame);
        }
    }
    void OnVolumeChanged(float) override {}
    void OnMuteDetected(bool) override {}
    void OnCaptureError(ErrorCode, const std::string&) override {}
    void OnDeviceChanged() override {}
};

// 视频渲染观察者
class VideoRenderCallback : public VideoRenderObserver {
public:
    void OnFrameRendered(const VideoFrame&, int64_t) override {}
    void OnRenderError(ErrorCode, const std::string&) override {}
    void OnRenderStats(const VideoRenderStats&) override {}
};

// 音频播放观察者
class AudioPlayCallback : public AudioPlayObserver {
public:
    void OnFramePlayed(const AudioFrame&, int64_t) override {}
    void OnPlayError(ErrorCode, const std::string&) override {}
    void OnPlayStats(const AudioPlayStats&) override {}
    void OnBufferingChanged(bool) override {}
    void OnMuteChanged(bool) override {}
};

class MiniRTCClient {
public:
    bool Initialize() {
        // 1. 初始化库
        if (minirtc_init() != MINIRTC_OK) {
            std::cerr << "Failed to init MiniRTC" << std::endl;
            return false;
        }
        
        std::cout << "MiniRTC " << minirtc_version() << " initialized" << std::endl;
        
        // 2. 初始化视频采集
        video_capture_ = CaptureFactory::CreateVideoCapture(
            CaptureFactory::CaptureType::kFake
        );
        
        VideoCaptureParam video_param;
        video_param.width = 1280;
        video_param.height = 720;
        video_param.fps = 30;
        video_capture_->SetParam(video_param);
        
        // 3. 初始化音频采集
        audio_capture_ = CaptureFactory::CreateAudioCapture(
            CaptureFactory::CaptureType::kFake
        );
        
        AudioCaptureParam audio_param;
        audio_param.sample_rate = 48000;
        audio_param.channels = 2;
        audio_capture_->SetParam(audio_param);
        
        // 4. 初始化视频渲染
        video_renderer_ = RendererFactory::CreateVideoRenderer(
            RendererFactory::RendererType::kFake
        );
        
        VideoRenderParam render_param;
        render_param.width = 1280;
        render_param.height = 720;
        render_param.pixel_format = VideoPixelFormat::kRGBA;
        video_renderer_->SetParam(render_param);
        
        // 5. 初始化音频播放
        audio_player_ = RendererFactory::CreateAudioPlayer(
            RendererFactory::RendererType::kFake
        );
        
        AudioPlayParam play_param;
        play_param.sample_rate = 48000;
        play_param.channels = 2;
        play_param.buffering_ms = 50;
        audio_player_->SetParam(play_param);
        
        // 6. 初始化编解码器
        auto& factory = CodecFactory::Instance();
        
        VideoEncoderConfig enc_cfg;
        enc_cfg.codec_type = CodecType::kH264;
        enc_cfg.width = 1280;
        enc_cfg.height = 720;
        enc_cfg.bitrate_kbps = 2000;
        enc_cfg.frame_rate = 30;
        video_encoder_ = factory.CreateEncoder(enc_cfg);
        
        VideoDecoderConfig dec_cfg;
        dec_cfg.codec_type = CodecType::kH264;
        video_decoder_ = factory.CreateDecoder(dec_cfg);
        
        AudioEncoderConfig audio_enc_cfg;
        audio_enc_cfg.codec_type = CodecType::kOpus;
        audio_enc_cfg.sample_rate = 48000;
        audio_enc_cfg.channels = 2;
        audio_encoder_ = factory.CreateEncoder(audio_enc_cfg);
        
        AudioDecoderConfig audio_dec_cfg;
        audio_dec_cfg.codec_type = CodecType::kOpus;
        audio_decoder_ = factory.CreateDecoder(audio_dec_cfg);
        
        // 7. 初始化抖动缓冲
        JitterBufferConfig jb_config;
        jb_config.min_latency_ms = 50;
        jb_config.max_latency_ms = 200;
        jb_config.target_latency_ms = 100;
        jb_config.enable_adaptive = true;
        jitter_buffer_ = std::make_unique<JitterBuffer>();
        jitter_buffer_->Initialize(jb_config);
        
        return true;
    }
    
    bool StartCall() {
        // 初始化所有组件
        ErrorCode result;
        
        result = video_capture_->Initialize();
        if (result != ErrorCode::kOk) {
            std::cerr << "Failed to init video capture: " << (int)result << std::endl;
            return false;
        }
        
        result = audio_capture_->Initialize();
        if (result != ErrorCode::kOk) {
            std::cerr << "Failed to init audio capture: " << (int)result << std::endl;
            return false;
        }
        
        result = video_renderer_->Initialize();
        if (result != ErrorCode::kOk) {
            std::cerr << "Failed to init video renderer: " << (int)result << std::endl;
            return false;
        }
        
        result = audio_player_->Initialize();
        if (result != ErrorCode::kOk) {
            std::cerr << "Failed to init audio player: " << (int)result << std::endl;
            return false;
        }
        
        // 设置视频采集回调
        video_observer_ = std::make_shared<VideoCaptureCallback>();
        video_observer_->on_frame = [this](const VideoFrame& frame) {
            this->OnVideoCaptured(frame);
        };
        
        // 设置音频采集回调
        audio_observer_ = std::make_shared<AudioCaptureCallback>();
        audio_observer_->on_frame = [this](const AudioFrame& frame) {
            this->OnAudioCaptured(frame);
        };
        
        // 开始采集
        result = video_capture_->StartCapture(video_observer_);
        if (result != ErrorCode::kOk) {
            std::cerr << "Failed to start video capture: " << (int)result << std::endl;
            return false;
        }
        
        result = audio_capture_->StartCapture(audio_observer_.get());
        if (result != ErrorCode::kOk) {
            std::cerr << "Failed to start audio capture: " << (int)result << std::endl;
            return false;
        }
        
        // 开始渲染
        result = video_renderer_->StartRender(new VideoRenderCallback());
        if (result != ErrorCode::kOk) {
            std::cerr << "Failed to start video render: " << (int)result << std::endl;
            return false;
        }
        
        result = audio_player_->StartPlay(new AudioPlayCallback());
        if (result != ErrorCode::kOk) {
            std::cerr << "Failed to start audio play: " << (int)result << std::endl;
            return false;
        }
        
        std::cout << "Call started successfully!" << std::endl;
        return true;
    }
    
    void StopCall() {
        // 停止采集
        video_capture_->StopCapture();
        audio_capture_->StopCapture();
        
        // 停止渲染
        video_renderer_->StopRender();
        audio_player_->StopPlay();
        
        // 释放资源
        video_capture_->Release();
        audio_capture_->Release();
        video_renderer_->Release();
        audio_player_->Release();
        
        std::cout << "Call stopped" << std::endl;
    }
    
    void Shutdown() {
        minirtc_shutdown();
        std::cout << "MiniRTC shutdown" << std::endl;
    }

private:
    void OnVideoCaptured(const VideoFrame& frame) {
        // 1. 编码视频帧
        EncoderFrame enc_frame;
        enc_frame.width = frame.width;
        enc_frame.height = frame.height;
        enc_frame.format = VideoPixelFormat::kI420;  // 假设已转换为I420
        enc_frame.timestamp_us = frame.timestamp_us;
        enc_frame.data = frame.data;
        
        std::vector<EncoderFrame> encoded_frames;
        ErrorCode result = video_encoder_->Encode(enc_frame, &encoded_frames);
        
        if (result == ErrorCode::kOk) {
            for (const auto& enc : encoded_frames) {
                // 2. 发送 RTP 包（此处为模拟）
                // SendRtpPacket(enc.data, enc.data_size, enc.timestamp_us, enc.keyframe);
                std::cout << "Video encoded: " << (enc.keyframe ? "KEY" : "P") 
                          << ", " << enc.data_size << " bytes" << std::endl;
            }
        }
    }
    
    void OnAudioCaptured(const AudioFrame& frame) {
        // 1. 编码音频帧
        EncoderFrame enc_frame;
        enc_frame.sample_rate = frame.sample_rate;
        enc_frame.channels = frame.channels;
        enc_frame.samples_per_channel = frame.samples_per_channel;
        enc_frame.timestamp_us = frame.timestamp_us;
        enc_frame.data = frame.data;
        
        std::vector<EncoderFrame> encoded_frames;
        ErrorCode result = audio_encoder_->Encode(enc_frame, &encoded_frames);
        
        if (result == ErrorCode::kOk) {
            for (const auto& enc : encoded_frames) {
                // 2. 发送 RTP 包（此处为模拟）
                // SendRtpPacket(enc.data, enc.data_size, enc.timestamp_us);
                std::cout << "Audio encoded: " << enc.data_size << " bytes" << std::endl;
            }
        }
    }

    // 模块实例
    std::unique_ptr<IVideoCapture> video_capture_;
    std::unique_ptr<IAudioCapture> audio_capture_;
    std::unique_ptr<IVideoRenderer> video_renderer_;
    std::unique_ptr<IAudioPlayer> audio_player_;
    std::unique_ptr<IEncoder> video_encoder_;
    std::unique_ptr<IDecoder> video_decoder_;
    std::unique_ptr<IEncoder> audio_encoder_;
    std::unique_ptr<IDecoder> audio_decoder_;
    std::unique_ptr<JitterBuffer> jitter_buffer_;
    
    std::shared_ptr<VideoCaptureCallback> video_observer_;
    std::shared_ptr<AudioCaptureCallback> audio_observer_;
};

int main() {
    std::cout << "=== MiniRTC Simple Call Demo ===" << std::endl;
    
    MiniRTCClient client;
    
    // 初始化
    if (!client.Initialize()) {
        return -1;
    }
    
    // 开始通话
    if (!client.StartCall()) {
        client.Shutdown();
        return -1;
    }
    
    std::cout << "Press Enter to end the call..." << std::endl;
    std::getchar();
    
    // 结束通话
    client.StopCall();
    client.Shutdown();
    
    return 0;
}
```

### 代码说明

1. **初始化阶段**：
   - 调用 `minirtc_init()` 初始化库
   - 创建采集器、渲染器、编码器、解码器实例
   - 配置各模块参数

2. **通话阶段**：
   - 启动采集和渲染
   - 采集回调中获取原始帧
   - 编码后通过网络发送

3. **结束阶段**：
   - 停止所有模块
   - 释放资源
   - 调用 `minirtc_shutdown()`

---

## 4. 常见问题

### Q1: 如何选择 Fake 和 Platform 采集器？

- **Fake 采集器**：用于测试和开发，不依赖真实硬件
- **Platform 采集器**：使用系统真实设备，需要平台特定实现

```cpp
// 测试/开发
auto capture = CaptureFactory::CreateVideoCapture(
    CaptureFactory::CaptureType::kFake
);

// 生产环境
auto capture = CaptureFactory::CreateVideoCapture(
    CaptureFactory::CaptureType::kPlatform
);
```

### Q2: 如何处理编解码器创建失败？

```cpp
auto encoder = factory.CreateEncoder(config);
if (!encoder) {
    std::cerr << "Encoder not available for this codec type" << std::endl;
    // 回退到其他编解码器
}
```

### Q3: 如何调整延迟和流畅度？

通过抖动缓冲配置调整：

```cpp
JitterBufferConfig config;
config.min_latency_ms = 30;      // 降低延迟
config.max_latency_ms = 100;
config.target_latency_ms = 50;
config.enable_adaptive = true;   // 自适应
jitter_buffer_->Initialize(config);
```

- **低延迟**：设置较小的 latency 值（30-50ms），但可能出现卡顿
- **高流畅**：设置较大的 latency 值（100-200ms），但延迟增加

### Q4: 如何获取设备列表？

```cpp
std::vector<VideoDeviceInfo> devices;
capture->GetDevices(&devices);

for (const auto& dev : devices) {
    std::cout << "Device: " << dev.device_name 
              << " (ID: " << dev.device_id << ")" << std::endl;
}
```

---

## 下一步

- 查看 [API 参考文档](API_REFERENCE.md) 了解更多细节
- 查看测试用例学习各模块的详细用法
- 根据你的需求组合不同的模块
