# MiniRTC 编解码模块接口设计文档 v3.0

**版本**: 3.0
**日期**: 2026-03-10
**状态**: 新增
**前置版本**: v2.0

---

## 1. 概述

本文档定义了MiniRTC v3.0中编解码模块的详细接口设计。编解码模块是媒体流水线的核心组件，负责音视频数据的压缩与解压缩处理。

### 1.1 设计目标

- **统一接口**: 提供统一的编解码器抽象接口，支持音频和视频编解码器
- **可扩展性**: 支持新增编解码器类型，无需修改现有代码
- **高性能**: 设计零拷贝接口，最小化内存拷贝开销
- **线程安全**: 支持多线程编解码场景

### 1.2 模块架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Codec Factory                                │
│                   (CodecFactory - 工厂模式)                          │
└──────────────────────────┬──────────────────────────────────────────┘
                           │
        ┌──────────────────┼──────────────────┐
        ▼                  ▼                  ▼
┌───────────────┐  ┌───────────────┐  ┌───────────────┐
│  ICodec       │  │  IEncoder     │  │  IDecoder     │
│  (基类)       │  │  (编码器)     │  │  (解码器)     │
└───────┬───────┘  └───────┬───────┘  └───────┬───────┘
        │                  │                  │
        ▼                  ▼                  ▼
┌───────────────┐  ┌───────────────┐  ┌───────────────┐
│ AudioCodec    │  │ AudioEncoder  │  │ AudioDecoder  │
│ (Opus)        │  │ (Opus)        │  │ (Opus)        │
└───────────────┘  └───────────────┘  └───────────────┘

┌───────────────┐  ┌───────────────┐  ┌───────────────┐
│ VideoCodec    │  │ VideoEncoder  │  │ VideoDecoder  │
│ (H.264)       │  │ (H.264)       │  │ (H.264)       │
└───────────────┘  └───────────────┘  └───────────────┘
```

---

## 2. ICodec 基类设计

### 2.1 基础类型定义

```cpp
#ifndef MINIRTC_CODEC_TYPES_H_
#define MINIRTC_CODEC_TYPES_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace minirtc {

/// 编解码器类型
enum class CodecType {
  kNone = 0,
  // 音频编解码器
  kOpus = 1,
  kAAC = 2,
  kG722 = 3,
  kPCMU = 4,
  kPCMA = 5,
  // 视频编解码器
  kH264 = 100,
  kH265 = 101,
  kVP8 = 102,
  kVP9 = 103,
  kAV1 = 104,
};

/// 媒体类型
enum class MediaType {
  kNone = 0,
  kAudio = 1,
  kVideo = 2,
};

/// 编解码器状态
enum class CodecState {
  kUninitialized = 0,
  kInitialized = 1,
  kRunning = 2,
  kPaused = 3,
  kStopped = 4,
  kError = 5,
};

/// 编码质量等级
enum class EncodeQuality {
  kLow = 0,       // 低码率，适合弱网
  kMedium = 1,    // 中等码率，平衡质量与带宽
  kHigh = 2,      // 高码率，高质量
  kUltra = 3,     // 超高质量
};

/// 码率控制模式
enum class BitrateControl {
  kCBR = 0,       // 固定码率
  kVBR = 1,       // 可变码率
  kCBRHQ = 2,     // 高质量固定码率
  kVBRHQ = 3,     // 高质量可变码率
};

/// 像素格式
enum class VideoPixelFormat {
  kI420 = 0,
  kNV12 = 1,
  kNV21 = 2,
  kBGRA = 3,
  kRGBA = 4,
  kYUY2 = 5,
  kMJPEG = 6,
};

/// 音频采样格式
enum class AudioSampleFormat {
  kS16 = 0,       // 16位有符号整数
  kS16LE = 0,     // 16位小端
  kS16BE = 1,     // 16位大端
  kF32 = 2,       // 32位浮点
  kF32LE = 2,     // 32位小端浮点
  kF32BE = 3,     // 32位大端浮点
};

/// 编解码错误码
enum class CodecError {
  kOk = 0,
  kInvalidParam = -1,
  kNotInitialized = -2,
  kAlreadyInitialized = -3,
  kNotSupported = -4,
  kOutOfMemory = -5,
  kHardwareError = -6,
  kStreamError = -7,
  kBufferTooSmall = -8,
  kTimeout = -9,
};

/// 视频帧信息
struct VideoFrameInfo {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t stride[4] = {0, 0, 0, 0};  // YUV各平面步长
  VideoPixelFormat format = VideoPixelFormat::kI420;
  uint64_t timestamp_us = 0;        // 微秒时间戳
  bool keyframe = false;
  int rotation = 0;                 // 旋转角度 (0, 90, 180, 270)
};

/// 音频帧信息
struct AudioFrameInfo {
  uint32_t sample_rate = 48000;
  uint32_t channels = 2;
  uint32_t samples_per_channel = 0;
  AudioSampleFormat format = AudioSampleFormat::kS16;
  uint64_t timestamp_us = 0;
  bool speech = false;              // 是否语音信号
  bool music = false;               // 是否音乐信号
};

/// 编解码统计信息
struct CodecStats {
  uint64_t encoded_frames = 0;
  uint64_t decoded_frames = 0;
  uint64_t encoded_bytes = 0;
  uint64_t decoded_bytes = 0;
  uint32_t encode_time_us = 0;     // 编码耗时 (微秒)
  uint32_t decode_time_us = 0;     // 解码耗时 (微秒)
  uint32_t last_bitrate_kbps = 0;
  double encode_fps = 0.0;
  double decode_fps = 0.0;
};

}  // namespace minirtc

#endif  // MINIRTC_CODEC_TYPES_H_
```

### 2.2 ICodec 接口定义

```cpp
#ifndef MINIRTC_ICODEC_H_
#define MINIRTC_ICODEC_H_

#include "codec_types.h"
#include <functional>

namespace minirtc {

/**
 * @brief 编解码器配置基类
 */
class ICodecConfig {
 public:
  virtual ~ICodecConfig() = default;
  
  /// 获取编解码器类型
  virtual CodecType GetType() const = 0;
  
  /// 获取媒体类型
  virtual MediaType GetMediaType() const = 0;
  
  /// 克隆配置
  virtual std::unique_ptr<ICodecConfig> Clone() const = 0;
  
  /// 转换为JSON字符串
  virtual std::string ToString() const = 0;
  
  /// 从JSON字符串解析
  virtual bool FromString(const std::string& json) = 0;
};

/**
 * @brief 编解码器回调接口
 */
class ICodecCallback {
 public:
  virtual ~ICodecCallback() = default;
  
  /// 编码完成回调
  virtual void OnEncoded(std::shared_ptr<EncodedFrame> frame) = 0;
  
  /// 解码完成回调
  virtual void OnDecoded(std::shared_ptr<RawFrame> frame) = 0;
  
  /// 错误回调
  virtual void OnError(CodecError error, const std::string& message) = 0;
  
  /// 统计信息回调
  virtual void OnStats(const CodecStats& stats) = 0;
};

/**
 * @brief 编解码器基类接口
 * 
 * 所有编解码器实现必须继承此接口
 */
class ICodec {
 public:
  using Ptr = std::shared_ptr<ICodec>;
  
  virtual ~ICodec() = default;
  
  // ===== 基础操作 =====
  
  /// 初始化编解码器
  virtual CodecError Initialize(const ICodecConfig& config) = 0;
  
  /// 释放编解码器资源
  virtual CodecError Release() = 0;
  
  /// 重置编解码器状态
  virtual CodecError Reset() = 0;
  
  // ===== 状态查询 =====
  
  /// 获取编解码器类型
  virtual CodecType GetType() const = 0;
  
  /// 获取媒体类型
  virtual MediaType GetMediaType() const = 0;
  
  /// 获取当前状态
  virtual CodecState GetState() const = 0;
  
  /// 获取配置信息
  virtual std::unique_ptr<ICodecConfig> GetConfig() const = 0;
  
  // ===== 统计 =====
  
  /// 获取统计信息
  virtual CodecStats GetStats() const = 0;
  
  /// 重置统计信息
  virtual void ResetStats() = 0;
  
  // ===== 能力查询 =====
  
  /// 检查是否支持指定配置
  virtual bool IsSupported(const ICodecConfig& config) const = 0;
  
  /// 获取支持的编解码器列表
  static std::vector<CodecType> GetSupportedCodecs(MediaType media_type);
  
  /// 获取编解码器名称
  static std::string GetCodecName(CodecType type);
  
 protected:
  ICodec() = default;
  
  // 禁止拷贝
  ICodec(const ICodec&) = delete;
  ICodec& operator=(const ICodec&) = delete;
};

}  // namespace minirtc

#endif  // MINIRTC_ICODEC_H_
```

---

## 3. IEncoder 接口设计

### 3.1 编码数据帧定义

```cpp
#ifndef MINIRTC_ENCODER_FRAME_H_
#define MINIRTC_ENCODER_FRAME_H_

#include "codec_types.h"
#include <vector>
#include <memory>

namespace minirtc {

/**
 * @brief 原始未编码帧接口
 * 
 * 支持零拷贝视图，避免不必要的数据拷贝
 */
class IRawFrame : public std::enable_shared_from_this<IRawFrame> {
 public:
  using Ptr = std::shared_ptr<IRawFrame>;
  
  virtual ~IRawFrame() = default;
  
  // ===== 数据访问 =====
  
  /// 获取视频帧信息
  virtual const VideoFrameInfo& GetVideoInfo() const = 0;
  
  /// 获取音频帧信息
  virtual const AudioFrameInfo& GetAudioInfo() const = 0;
  
  /// 获取数据指针 (Y平面或音频样本)
  virtual const uint8_t* GetData() const = 0;
  
  /// 获取数据大小
  virtual size_t GetSize() const = 0;
  
  /// 获取 Plane 数据 (视频)
  virtual const uint8_t* GetPlaneData(int plane) const = 0;
  virtual int GetPlaneSize(int plane) const = 0;
  
  // ===== 数据管理 =====
  
  /// 设置数据引用 (零拷贝)
  virtual void SetDataRef(uint8_t* data, size_t size, 
                         std::function<void(uint8_t*)> releaser = nullptr) = 0;
  
  /// 复制数据
  virtual Ptr Clone() const = 0;
  
  // ===== 时间戳 =====
  
  /// 获取时间戳 (微秒)
  virtual uint64_t GetTimestampUs() const = 0;
  
  /// 设置时间戳
  virtual void SetTimestampUs(uint64_t ts) = 0;
};

/**
 * @brief 编码后帧接口
 */
class IEncodedFrame : public std::enable_shared_from_this<IEncodedFrame> {
 public:
  using Ptr = std::shared_ptr<IEncodedFrame>;
  
  virtual ~IEncodedFrame() = default;
  
  // ===== 基础信息 =====
  
  /// 获取数据指针
  virtual const uint8_t* GetData() const = 0;
  
  /// 获取数据大小
  virtual size_t GetSize() const = 0;
  
  /// 是否是关键帧
  virtual bool IsKeyframe() const = 0;
  
  /// 获取时间戳 (微秒)
  virtual uint64_t GetTimestampUs() const = 0;
  
  /// 获取帧序号
  virtual uint32_t GetFrameNumber() const = 0;
  
  // ===== RTP相关信息 =====
  
  /// 获取SSRC
  virtual uint32_t GetSSRC() const = 0;
  
  /// 设置SSRC
  virtual void SetSSRC(uint32_t ssrc) = 0;
  
  /// 获取 RTP 序列号
  virtual uint16_t GetSeqNum() const = 0;
  
  /// 设置 RTP 序列号
  virtual void SetSeqNum(uint16_t seq) = 0;
  
  /// 获取 RTP 时间戳
  virtual uint32_t GetRtpTimestamp() const = 0;
  
  /// 设置 RTP 时间戳
  virtual void SetRtpTimestamp(uint32_t ts) = 0;
  
  /// 获取负载类型
  virtual uint8_t GetPayloadType() const = 0;
  
  /// 设置负载类型
  virtual void SetPayloadType(uint8_t pt) = 0;
  
  // ===== NAL单元 (H.264) =====
  
  /// 获取 NAL 单元列表
  virtual const std::vector<uint8_t>& GetNALUnits() const = 0;
  
  /// 添加 NAL 单元
  virtual void AddNALUnit(const uint8_t* data, size_t size) = 0;
  
  // ===== 数据管理 =====
  
  /// 设置数据引用 (零拷贝)
  virtual void SetDataRef(uint8_t* data, size_t size,
                         std::function<void(uint8_t*)> releaser = nullptr) = 0;
  
  /// 复制数据
  virtual Ptr Clone() const = 0;
  
  // ===== 缓冲管理 =====
  
  /// 预留缓冲区空间
  virtual bool ReserveBuffer(size_t size) = 0;
  
  /// 获取已预留的缓冲区大小
  virtual size_t GetBufferCapacity() const = 0;
};

}  // namespace minirtc

#endif  // MINIRTC_ENCODER_FRAME_H_
```

### 3.2 编码器配置

```cpp
#ifndef MINIRTC_ENCODER_CONFIG_H_
#define MINIRTC_ENCODER_CONFIG_H_

#include "icodec.h"
#include <string>

namespace minirtc {

/**
 * @brief 视频编码器配置
 */
struct VideoEncoderConfig : public ICodecConfig {
  CodecType type = CodecType::kH264;
  MediaType media_type = MediaType::kVideo;
  
  // 分辨率
  uint32_t width = 1280;
  uint32_t height = 720;
  
  // 帧率
  uint32_t framerate = 30;
  uint32_t keyframe_interval = 60;  // 关键帧间隔
  
  // 码率控制
  uint32_t target_bitrate_kbps = 1000;  // 目标码率 (kbps)
  uint32_t max_bitrate_kbps = 2000;     // 最大码率 (kbps)
  BitrateControl bitrate_control = BitrateControl::kVBR;
  
  // 编码质量
  EncodeQuality quality = EncodeQuality::kMedium;
  
  // 编码档次
  std::string profile = "high";    // baseline, main, high
  std::string level = "3.1";       // 3.1, 4.0, 4.1, etc.
  
  // 熵编码
  std::string entropy_mode = "cabac";  // cabac, cavlc
  
  // B帧配置
  int max_bframes = 0;
  int b_frame_ref = 0;
  
  // 硬件加速
  bool use_hardware = true;
  std::string hardware_device = "auto";  // auto, 0, 1, ...
  
  // 多线程
  int thread_count = 0;  // 0 = 自动
  
  // 复杂度
  int complexity = 50;   // 0-100
  
  // 场景自适应
  bool scene_change_detection = true;
  
  // 实现 ICodecConfig
  CodecType GetType() const override { return type; }
  MediaType GetMediaType() const override { return media_type; }
  
  std::unique_ptr<ICodecConfig> Clone() const override {
    return std::make_unique<VideoEncoderConfig>(*this);
  }
  
  std::string ToString() const override;
  bool FromString(const std::string& json) override;
};

/**
 * @brief 音频编码器配置
 */
struct AudioEncoderConfig : public ICodecConfig {
  CodecType type = CodecType::kOpus;
  MediaType media_type = MediaType::kAudio;
  
  // 采样率
  uint32_t sample_rate = 48000;
  
  // 声道数
  uint32_t channels = 2;
  
  // 比特率
  uint32_t bitrate_bps = 64000;  // bps
  
  // 编码复杂度
  int complexity = 10;  // 0-10
  
  // 信号类型
  bool force_channel_mapping = false;
  bool application_kbe = false;  // true = 音乐, false = 语音
  
  // 帧大小
  uint32_t frame_size_ms = 20;  // 5, 10, 20, 40, 60, 80, 100, 120
  
  // 带宽控制
  std::string bandwidth = "fullband";  // narrowband, mediumband, wideband, superwideband, fullband
  
  // VBR配置
  bool vbr = true;
  bool vbr_constraint = false;
  
  // 信号检测
  bool signal_detection = true;
  bool force_mode = false;  // 强制使用指定模式
  
  // 实现 ICodecConfig
  CodecType GetType() const override { return type; }
  MediaType GetMediaType() const override { return media_type; }
  
  std::unique_ptr<ICodecConfig> Clone() const override {
    return std::make_unique<AudioEncoderConfig>(*this);
  }
  
  std::string ToString() const override;
  bool FromString(const std::string& json) override;
};

}  // namespace minirtc

#endif  // MINIRTC_ENCODER_CONFIG_H_
```

### 3.3 IEncoder 接口定义

```cpp
#ifndef MINIRTC_IENCODER_H_
#define MINIRTC_IENCODER_H_

#include "icodec.h"
#include "encoder_frame.h"
#include "encoder_config.h"

namespace minirtc {

/**
 * @brief 编码器接口
 * 
 * 所有编码器实现必须继承此接口
 */
class IEncoder : public ICodec {
 public:
  using Ptr = std::shared_ptr<IEncoder>;
  
  // ===== 配置 =====
  
  /// 设置配置 (运行时可调)
  virtual CodecError SetConfig(const VideoEncoderConfig& config) = 0;
  virtual CodecError SetConfig(const AudioEncoderConfig& config) = 0;
  
  /// 获取配置
  virtual std::unique_ptr<VideoEncoderConfig> GetVideoConfig() const = 0;
  virtual std::unique_ptr<AudioEncoderConfig> GetAudioConfig() const = 0;
  
  // ===== 编码操作 =====
  
  /// 编码单帧
  /// @param input 输入原始帧
  /// @param output 输出编码帧
  /// @return 错误码
  virtual CodecError Encode(std::shared_ptr<IRawFrame> input,
                           std::shared_ptr<IEncodedFrame>* output) = 0;
  
  /// 编码多帧 (批处理)
  virtual CodecError EncodeBatch(const std::vector<std::shared_ptr<IRawFrame>>& inputs,
                                 std::vector<std::shared_ptr<IEncodedFrame>>* outputs) = 0;
  
  /// 刷新编码器 (获取所有pending的编码数据)
  virtual CodecError Flush(std::vector<std::shared_ptr<IEncodedFrame>>* outputs) = 0;
  
  // ===== 速率控制 =====
  
  /// 请求关键帧
  virtual void RequestKeyframe() = 0;
  
  /// 设置目标码率
  virtual CodecError SetBitrate(uint32_t target_kbps, uint32_t max_kbps) = 0;
  
  /// 设置帧率
  virtual CodecError SetFramerate(uint32_t fps) = 0;
  
  /// 设置编码质量
  virtual CodecError SetQuality(EncodeQuality quality) = 0;
  
  // ===== 回调 =====
  
  /// 设置编码回调
  virtual void SetCallback(ICodecCallback* callback) = 0;
  
  // ===== 能力查询 =====
  
  /// 获取支持的最大分辨率
  virtual void GetSupportedResolutions(std::vector<std::pair<uint32_t, uint32_t>>* resolutions) const = 0;
  
  /// 获取支持的最大帧率
  virtual uint32_t GetMaxFramerate() const = 0;
  
  /// 获取支持的最大码率 (kbps)
  virtual uint32_t GetMaxBitrate() const = 0;
  
  /// 检查硬件加速是否可用
  virtual bool IsHardwareAccelerationAvailable() const = 0;
  
 protected:
  IEncoder() = default;
};

}  // namespace minirtc

#endif  // MINIRTC_IENCODER_H_
```

---

## 4. IDecoder 接口设计

### 4.1 解码器配置

```cpp
#ifndef MINIRTC_DECODER_CONFIG_H_
#define MINIRTC_DECODER_CONFIG_H_

#include "icodec.h"

namespace minirtc {

/**
 * @brief 视频解码器配置
 */
struct VideoDecoderConfig : public ICodecConfig {
  CodecType type = CodecType::kH264;
  MediaType media_type = MediaType::kVideo;
  
  // 输出格式
  VideoPixelFormat output_format = VideoPixelFormat::kI420;
  
  // 线程配置
  int thread_count = 0;  // 0 = 自动
  
  // 硬件加速
  bool use_hardware = true;
  std::string hardware_device = "auto";
  
  // 错误隐藏
  bool error_concealment = true;
  
  // 低延迟模式
  bool low_latency = false;
  
  // 错误恢复
  bool enable_frame_drop = true;
  
  // 实现 ICodecConfig
  CodecType GetType() const override { return type; }
  MediaType GetMediaType() const override { return media_type; }
  
  std::unique_ptr<ICodecConfig> Clone() const override {
    return std::make_unique<VideoDecoderConfig>(*this);
  }
  
  std::string ToString() const override;
  bool FromString(const std::string& json) override;
};

/**
 * @brief 音频解码器配置
 */
struct AudioDecoderConfig : public ICodecConfig {
  CodecType type = CodecType::kOpus;
  MediaType media_type = MediaType::kAudio;
  
  // 输出格式
  AudioSampleFormat output_format = AudioSampleFormat::kS16;
  
  // 输出采样率 (0 = 与输入相同)
  uint32_t output_sample_rate = 0;
  
  // 输出声道数 (0 = 与输入相同)
  uint32_t output_channels = 0;
  
  // 声道重映射
  std::vector<int> channel_mapping;
  
  // 错误隐藏
  bool packet_loss_concealment = true;
  
  // 实现 ICodecConfig
  CodecType GetType() const override { return type; }
  MediaType GetMediaType() const override { return media_type; }
  
  std::unique_ptr<ICodecConfig> Clone() const override {
    return std::make_unique<AudioDecoderConfig>(*this);
  }
  
  std::string ToString() const override;
  bool FromString(const std::string& json) override;
};

}  // namespace minirtc

#endif  // MINIRTC_DECODER_CONFIG_H_
```

### 4.2 IDecoder 接口定义

```cpp
#ifndef MINIRTC_IDECODER_H_
#define MINIRTC_IDECODER_H_

#include "icodec.h"
#include "encoder_frame.h"
#include "decoder_config.h"

namespace minirtc {

/**
 * @brief 解码器接口
 * 
 * 所有解码器实现必须继承此接口
 */
class IDecoder : public ICodec {
 public:
  using Ptr = std::shared_ptr<IDecoder>;
  
  // ===== 配置 =====
  
  /// 设置配置 (运行时可调)
  virtual CodecError SetConfig(const VideoDecoderConfig& config) = 0;
  virtual CodecError SetConfig(const AudioDecoderConfig& config) = 0;
  
  /// 获取配置
  virtual std::unique_ptr<VideoDecoderConfig> GetVideoConfig() const = 0;
  virtual std::unique_ptr<AudioDecoderConfig> GetAudioConfig() const = 0;
  
  // ===== 解码操作 =====
  
  /// 解码单帧
  /// @param input 输入编码帧
  /// @param output 输出原始帧
  /// @return 错误码
  virtual CodecError Decode(std::shared_ptr<IEncodedFrame> input,
                           std::shared_ptr<IRawFrame>* output) = 0;
  
  /// 解码多帧 (批处理)
  virtual CodecError DecodeBatch(const std::vector<std::shared_ptr<IEncodedFrame>>& inputs,
                                 std::vector<std::shared_ptr<IRawFrame>>* outputs) = 0;
  
  /// 解码原始数据 (如Annex-B格式)
  virtual CodecError DecodeRaw(const uint8_t* data, size_t size,
                               std::shared_ptr<IRawFrame>* output) = 0;
  
  /// 刷新解码器 (获取所有pending的解码数据)
  virtual CodecError Flush(std::vector<std::shared_ptr<IRawFrame>>* outputs) = 0;
  
  // ===== 解码器状态 =====
  
  /// 设置SPS/PPS (H.264)
  virtual CodecError SetParameterSets(const uint8_t* sps, size_t sps_size,
                                      const uint8_t* pps, size_t pps_size) = 0;
  
  /// 获取当前SPS
  virtual std::vector<uint8_t> GetSPS() const = 0;
  
  /// 获取当前PPS
  virtual std::vector<uint8_t> GetPPS() const = 0;
  
  /// 检查是否已收到SPS/PPS
  virtual bool HasParameterSets() const = 0;
  
  // ===== 丢包处理 =====
  
  /// 设置丢包率 (用于PLC)
  virtual void SetPacketLossRate(double loss_rate) = 0;
  
  /// 通知丢包
  virtual void NotifyPacketLost(uint16_t seq_start, uint16_t seq_end) = 0;
  
  // ===== 回调 =====
  
  /// 设置解码回调
  virtual void SetCallback(ICodecCallback* callback) = 0;
  
  // ===== 能力查询 =====
  
  /// 获取支持的最大分辨率
  virtual void GetSupportedResolutions(std::vector<std::pair<uint32_t, uint32_t>>* resolutions) const = 0;
  
  /// 检查硬件加速是否可用
  virtual bool IsHardwareAccelerationAvailable() const = 0;
  
 protected:
  IDecoder() = default;
};

}  // namespace minirtc

#endif  // MINIRTC_IDECODER_H_
```

---

## 5. AudioCodec (Opus) 接口设计

### 5.1 Opus编码器实现

```cpp
#ifndef MINIRTC_OPUS_ENCODER_H_
#define MINIRTC_OPUS_ENCODER_H_

#include "iencoder.h"
#include "encoder_config.h"
#include <opus.h>

namespace minirtc {

/**
 * @brief Opus音频编码器
 * 
 * 基于libopus实现，支持VoIP和音乐模式
 */
class OpusEncoder : public IEncoder {
 public:
  OpusEncoder();
  ~OpusEncoder() override;
  
  // ===== ICodec 接口实现 =====
  
  CodecError Initialize(const ICodecConfig& config) override;
  CodecError Release() override;
  CodecError Reset() override;
  
  CodecType GetType() const override { return CodecType::kOpus; }
  MediaType GetMediaType() const override { return MediaType::kAudio; }
  CodecState GetState() const override { return state_; }
  std::unique_ptr<ICodecConfig> GetConfig() const override;
  
  CodecStats GetStats() const override;
  void ResetStats() override;
  
  bool IsSupported(const ICodecConfig& config) const override;
  
  // ===== IEncoder 接口实现 =====
  
  CodecError SetConfig(const AudioEncoderConfig& config) override;
  std::unique_ptr<AudioEncoderConfig> GetAudioConfig() const override;
  
  CodecError Encode(std::shared_ptr<IRawFrame> input,
                   std::shared_ptr<IEncodedFrame>* output) override;
  CodecError EncodeBatch(const std::vector<std::shared_ptr<IRawFrame>>& inputs,
                        std::vector<std::shared_ptr<IEncodedFrame>>* outputs) override;
  CodecError Flush(std::vector<std::shared_ptr<IEncodedFrame>>* outputs) override;
  
  void RequestKeyframe() override;  // 音频无关键帧，此接口空实现
  CodecError SetBitrate(uint32_t target_bps, uint32_t max_bps) override;
  CodecError SetFramerate(uint32_t fps) override;
  CodecError SetQuality(EncodeQuality quality) override;
  
  void SetCallback(ICodecCallback* callback) override;
  
  // ===== Opus特定接口 =====
  
  /// 设置Opus应用类型
  void SetApplication(opus_int32 application);  // OPUS_APPLICATION_VOIP or OPUS_APPLICATION_AUDIO
  
  /// 设置带宽
  void SetBandwidth(opus_int32 bandwidth);  // OPUS_BANDWIDTH_*
  
  /// 设置Force Channel Mapping
  void SetForceChannelMapping(bool force);
  
  /// 获取当前帧大小 (样本数)
  int GetFrameSize() const { return frame_size_; }
  
  /// 获取Opus内部状态 (用于高级操作)
  OpusEncoder* GetOpusState() { return this; }

 private:
  CodecError CreateEncoder();
  CodecError DestroyEncoder();
  CodecError UpdateEncoderSettings();
  
  OpusEncoder* opus_state_ = nullptr;
  AudioEncoderConfig config_;
  CodecState state_ = CodecState::kUninitialized;
  CodecStats stats_;
  ICodecCallback* callback_ = nullptr;
  
  int frame_size_ = 0;
  int lookahead_ = 0;
  uint64_t timestamp_us_ = 0;
  
  // 缓存
  std::vector<int16_t> input_buffer_;
  std::vector<uint8_t> output_buffer_;
};

/**
 * @brief Opus编码器创建函数
 */
std::unique_ptr<IEncoder> CreateOpusEncoder();

}  // namespace minirtc

#endif  // MINIRTC_OPUS_ENCODER_H_
```

### 5.2 Opus解码器实现

```cpp
#ifndef MINIRTC_OPUS_DECODER_H_
#define MINIRTC_OPUS_DECODER_H_

#include "idecoder.h"
#include "decoder_config.h"
#include <opus.h>

namespace minirtc {

/**
 * @brief Opus音频解码器
 * 
 * 基于libopus实现，支持PLC (Packet Loss Concealment)
 */
class OpusDecoder : public IDecoder {
 public:
  OpusDecoder();
  ~OpusDecoder() override;
  
  // ===== ICodec 接口实现 =====
  
  CodecError Initialize(const ICodecConfig& config) override;
  CodecError Release() override;
  CodecError Reset() override;
  
  CodecType GetType() const override { return CodecType::kOpus; }
  MediaType GetMediaType() const override { return MediaType::kAudio; }
  CodecState GetState() const override { return state_; }
  std::unique_ptr<ICodecConfig> GetConfig() const override;
  
  CodecStats GetStats() const override;
  void ResetStats() override;
  
  bool IsSupported(const ICodecConfig& config) const override;
  
  // ===== IDecoder 接口实现 =====
  
  CodecError SetConfig(const AudioDecoderConfig& config) override;
  std::unique_ptr<AudioDecoderConfig> GetAudioConfig() const override;
  
  CodecError Decode(std::shared_ptr<IEncodedFrame> input,
                   std::shared_ptr<IRawFrame>* output) override;
  CodecError DecodeBatch(const std::vector<std::shared_ptr<IEncodedFrame>>& inputs,
                        std::vector<std::shared_ptr<IRawFrame>>* outputs) override;
  CodecError DecodeRaw(const uint8_t* data, size_t size,
                      std::shared_ptr<IRawFrame>* output) override;
  CodecError Flush(std::vector<std::shared_ptr<IRawFrame>>* outputs) override;
  
  CodecError SetParameterSets(const uint8_t* sps, size_t sps_size,
                             const uint8_t* pps, size_t pps_size) override;
  std::vector<uint8_t> GetSPS() const override { return {}; }  // 音频无SPS
  std::vector<uint8_t> GetPPS() const override { return {}; }  // 音频无PPS
  bool HasParameterSets() const override { return true; }
  
  void SetPacketLossRate(double loss_rate) override;
  void NotifyPacketLost(uint16_t seq_start, uint16_t seq_end) override;
  
  void SetCallback(ICodecCallback* callback) override;
  
  // ===== Opus特定接口 =====
  
  /// 获取解码器DLL状态
  int GetOpusFinalRange() const;
  
  /// 获取当前带宽
  int GetBandwidth() const;
  
  /// 获取样本率
  int GetSampleRate() const { return config_.sample_rate; }
  
  /// 设置PLC模式
  void SetPLCEnabled(bool enabled);

 private:
  CodecError CreateDecoder();
  CodecError DestroyDecoder();
  
  AudioDecoderConfig config_;
  OpusDecoder* decoder_state_ = nullptr;
  CodecState state_ = CodecState::kUninitialized;
  CodecStats stats_;
  ICodecCallback* callback_ = nullptr;
  
  // PLC状态
  bool plc_enabled_ = true;
  double packet_loss_rate_ = 0.0;
  
  // 输出缓冲
  std::vector<int16_t> output_buffer_;
  int output_samples_ = 0;
  
  // 状态跟踪
  int last_seq_ = -1;
};

/**
 * @brief Opus解码器创建函数
 */
std::unique_ptr<IDecoder> CreateOpusDecoder();

}  // namespace minirtc

#endif  // MINIRTC_OPUS_DECODER_H_
```

---

## 6. VideoCodec (H.264) 接口设计

### 6.1 H.264编码器实现

```cpp
#ifndef MINIRTC_H264_ENCODER_H_
#define MINIRTC_H264_ENCODER_H_

#include "iencoder.h"
#include "encoder_config.h"

#ifdef MINIRTC_USE_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#endif

namespace minirtc {

/**
 * @brief H.264视频编码器
 * 
 * 基于FFmpeg/libx264实现
 */
class H264Encoder : public IEncoder {
 public:
  H264Encoder();
  ~H264Encoder() override;
  
  // ===== ICodec 接口实现 =====
  
  CodecError Initialize(const ICodecConfig& config) override;
  CodecError Release() override;
  CodecError Reset() override;
  
  CodecType GetType() const override { return CodecType::kH264; }
  MediaType GetMediaType() const override { return MediaType::kVideo; }
  CodecState GetState() const override { return state_; }
  std::unique_ptr<ICodecConfig> GetConfig() const override;
  
  CodecStats GetStats() const override;
  void ResetStats() override;
  
  bool IsSupported(const ICodecConfig& config) const override;
  
  // ===== IEncoder 接口实现 =====
  
  CodecError SetConfig(const VideoEncoderConfig& config) override;
  std::unique_ptr<VideoEncoderConfig> GetVideoConfig() const override;
  
  CodecError Encode(std::shared_ptr<IRawFrame> input,
                   std::shared_ptr<IEncodedFrame>* output) override;
  CodecError EncodeBatch(const std::vector<std::EncodedFrame>* output) override;
  CodecError Flush(std::vector<std::shared_ptr<IEncodedFrame>>* outputs) override;
  
  void RequestKeyframe() override;
  CodecError SetBitrate(uint32_t target_kbps, uint32_t max_kbps) override;
  CodecError SetFramerate(uint32_t fps) override;
  CodecError SetQuality(EncodeQuality quality) override;
  
  void SetCallback(ICodecCallback* callback) override;
  
  // ===== H.264特定接口 =====
  
  /// 获取SPS
  virtual std::vector<uint8_t> GetSPS() const = 0;
  
  /// 获取PPS
  virtual std::vector<uint8_t> GetPPS() const = 0;
  
  /// 获取已编码的VPS (HEVC)
  virtual std::vector<uint8_t> GetVPS() const = 0;
  
  /// 启用/禁用瞬时刷新帧 (IDR)
  virtual void SetIDRRequestEnabled(bool enabled) = 0;
  
  /// 获取编码器状态
  virtual int GetEncoderState() const = 0;

 protected:
  H264Encoder() = default;
};

}  // namespace minirtc

#endif  // MINIRTC_H264_ENCODER_H_
```

### 6.2 H.264解码器实现

```cpp
#ifndef MINIRTC_H264_DECODER_H_
#define MINIRTC_H264_DECODER_H_

#include "idecoder.h"
#include "decoder_config.h"

#ifdef MINIRTC_USE_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#endif

namespace minirtc {

/**
 * @brief H.264视频解码器
 * 
 * 基于FFmpeg/libavcodec实现
 */
class H264Decoder : public IDecoder {
 public:
  H264Decoder();
  ~H264Decoder() override;
  
  // ===== ICodec 接口实现 =====
  
  CodecError Initialize(const ICodecConfig& config) override;
  CodecError Release() override;
  CodecError Reset() override;
  
  CodecType GetType() const override { return CodecType::kH264; }
  MediaType GetMediaType() const override { return MediaType::kVideo; }
  CodecState GetState() const override { return state_; }
  std::unique_ptr<ICodecConfig> GetConfig() const override;
  
  CodecStats GetStats() const override;
  void ResetStats() override;
  
  bool IsSupported(const ICodecConfig& config) const override;
  
  // ===== IDecoder 接口实现 =====
  
  CodecError SetConfig(const VideoDecoderConfig& config) override;
  std::unique_ptr<VideoDecoderConfig> GetVideoConfig() const override;
  
  CodecError Decode(std::shared_ptr<IEncodedFrame> input,
                   std::shared_ptr<IRawFrame>* output) override;
  CodecError DecodeBatch(const std::vector<std::shared_ptr<IEncodedFrame>>& inputs,
                        std::vector<std::shared_ptr<IRawFrame>>* outputs) override;
  CodecError DecodeRaw(const uint8_t* data, size_t size,
                      std::shared_ptr<IRawFrame>* output) override;
  CodecError Flush(std::vector<std::shared_ptr<IRawFrame>>* outputs) override;
  
  CodecError SetParameterSets(const uint8_t* sps, size_t sps_size,
                             const uint8_t* pps, size_t pps_size) override;
  std::vector<uint8_t> GetSPS() const override;
  std::vector<uint8_t> GetPPS() const override;
  bool HasParameterSets() const override;
  
  void SetPacketLossRate(double loss_rate) override;
  void NotifyPacketLost(uint16_t seq_start, uint16_t seq_end) override;
  
  void SetCallback(ICodecCallback* callback) override;
  
  // ===== H.264特定接口 =====
  
  /// 设置SEI消息回调
  using OnSEICallback = std::function<void(const uint8_t* data, size_t size)>;
  void SetSEICallback(OnSEICallback callback);
  
  /// 获取解码器DLL状态
  uint32_t GetDecoderFlags() const;
  
  /// 检查是否需要解码器重置
  bool NeedsDecoderReset() const;
  
  /// 获取当前解码图像数量
  int GetDecodingPictureCount() const;

 protected:
  VideoDecoderConfig config_;
  CodecState state_ = CodecState::kUninitialized;
  CodecStats stats_;
  ICodecCallback* callback_ = nullptr;
  
  // SPS/PPS缓存
  std::vector<uint8_t> sps_;
  std::vector<uint8_t> pps_;
  
  // SEI回调
  OnSEICallback sei_callback_;
};

/**
 * @brief H.264解码器创建函数
 */
std::unique_ptr<IDecoder> CreateH264Decoder();

}  // namespace minirtc

#endif  // MINIRTC_H264_DECODER_H_
```

---

## 7. 工厂模式设计

### 7.1 工厂接口定义

```cpp
#ifndef MINIRTC_CODEC_FACTORY_H_
#define MINIRTC_CODEC_FACTORY_H_

#include "icodec.h"
#include "iencoder.h"
#include "idecoder.h"
#include "encoder_config.h"
#include "decoder_config.h"
#include <map>
#include <memory>
#include <string>

namespace minirtc {

/**
 * @brief 编解码器工厂
 * 
 * 负责创建和管理编解码器实例
 */
class CodecFactory {
 public:
  using Ptr = std::shared_ptr<CodecFactory>;
  
  /**
   * @brief 获取工厂单例
   */
  static CodecFactory& Instance();
  
  // ===== 注册/注销编解码器 =====
  
  /// 注册编码器
  /// @param type 编解码器类型
  /// @param creator 创建函数
  void RegisterEncoder(CodecType type, 
                      std::function<std::unique_ptr<IEncoder>()> creator);
  
  /// 注册解码器
  void RegisterDecoder(CodecType type,
                      std::function<std::unique_ptr<IDecoder>()> creator);
  
  /// 注销编码器
  void UnregisterEncoder(CodecType type);
  
  /// 注销解码器
  void UnregisterDecoder(CodecType type);
  
  // ===== 创建编解码器 =====
  
  /// 创建编码器
  /// @param type 编解码器类型
  /// @param config 编码器配置
  /// @return 编解码器实例 nullptr表示失败
  std::unique_ptr<IEncoder> CreateEncoder(CodecType type,
                                          const ICodecConfig& config);
  
  /// 创建编码器 (通过配置结构体)
  std::unique_ptr<IEncoder> CreateEncoder(const VideoEncoderConfig& config);
  std::unique_ptr<IEncoder> CreateEncoder(const AudioEncoderConfig& config);
  
  /// 创建解码器
  std::unique_ptr<IDecoder> CreateDecoder(CodecType type,
                                          const ICodecConfig& config);
  std::unique_ptr<IDecoder> CreateDecoder(const VideoDecoderConfig& config);
  std::unique_ptr<IDecoder> CreateDecoder(const AudioDecoderConfig& config);
  
  // ===== 能力查询 =====
  
  /// 获取支持的编码器列表
  std::vector<CodecType> GetSupportedEncoders(MediaType media_type) const;
  
  /// 获取支持的解码器列表
  std::vector<CodecType> GetSupportedDecoders(MediaType media_type) const;
  
  /// 检查编码器是否支持指定配置
  bool IsEncoderSupported(CodecType type, const ICodecConfig& config) const;
  
  /// 检查解码器是否支持指定配置
  bool IsDecoderSupported(CodecType type, const ICodecConfig& config) const;
  
  // ===== 便捷方法 =====
  
  /// 根据SDP创建编码器
  std::unique_ptr<IEncoder> CreateEncoderFromSDP(const std::string& sdp,
                                                 MediaType media_type);
  
  /// 根据SDP创建解码器
  std::unique_ptr<IDecoder> CreateDecoderFromSDP(const std::string& sdp,
                                                  MediaType media_type);
  
 private:
  CodecFactory();
  ~CodecFactory() = default;
  
  // 禁止拷贝
  CodecFactory(const CodecFactory&) = delete;
  CodecFactory& operator=(const CodecFactory&) = delete;
  
  // 注册默认编解码器
  void RegisterDefaultCodecs();
  
  // 编解码器创建函数映射
  std::map<CodecType, std::function<std::unique_ptr<IEncoder>()>> encoder_creators_;
  std::map<CodecType, std::function<std::unique_ptr<IDecoder>()>> decoder_creators_;
};

/**
 * @brief 编解码器自动注册模板
 * 
 * 用于自动注册编解码器
 */
template<CodecType Type, typename EncoderImpl, typename DecoderImpl>
class CodecRegistrar {
 public:
  CodecRegistrar() {
    CodecFactory::Instance().RegisterEncoder(Type, []() {
      return std::make_unique<EncoderImpl>();
    });
    CodecFactory::Instance().RegisterDecoder(Type, []() {
      return std::make_unique<DecoderImpl>();
    });
  }
};

/**
 * @brief 便捷宏：注册编解码器
 * 
 * 使用示例:
 *   MINIRTC_REGISTER_CODEC(CodecType::kOpus, OpusEncoder, OpusDecoder);
 */
#define MINIRTC_REGISTER_CODEC(Type, EncoderClass, DecoderClass) \
  static ::minirtc::CodecRegistrar<Type, EncoderClass, DecoderClass> \
      g_##EncoderClass##_##DecoderClass##_registrar

}  // namespace minirtc

#endif  // MINIRTC_CODEC_FACTORY_H_
```

### 7.2 使用示例

```cpp
#include "codec_factory.h"
#include "opus_encoder.h"
#include "opus_decoder.h"
#include "h264_encoder.h"
#include "h264_decoder.h"

// 在程序入口处注册默认编解码器
MINIRTC_REGISTER_CODEC(CodecType::kOpus, OpusEncoder, OpusDecoder);
MINIRTC_REGISTER_CODEC(CodecType::kH264, H264Encoder, H264Decoder);

void Example() {
  auto& factory = CodecFactory::Instance();
  
  // 方法1: 使用配置结构体创建
  AudioEncoderConfig audio_config;
  audio_config.type = CodecType::kOpus;
  audio_config.bitrate_bps = 64000;
  audio_config.channels = 2;
  
  auto encoder = factory.CreateEncoder(audio_config);
  if (encoder) {
    encoder->Initialize(audio_config);
  }
  
  // 方法2: 直接指定类型
  VideoEncoderConfig video_config;
  video_config.type = CodecType::kH264;
  video_config.width = 1280;
  video_config.height = 720;
  video_config.framerate = 30;
  video_config.target_bitrate_kbps = 1000;
  
  auto video_encoder = factory.CreateEncoder(CodecType::kH264, video_config);
  
  // 方法3: 从SDP创建
  auto sdp_encoder = factory.CreateEncoderFromSDP(sdp_string, MediaType::kVideo);
}
```

---

## 8. 接口关系图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                            ICodec (基类)                                 │
├─────────────────────────────────────────────────────────────────────────┤
│  + Initialize(config)                                                   │
│  + Release()                                                            │
│  + Reset()                                                              │
│  + GetType(): CodecType                                                 │
│  + GetMediaType(): MediaType                                            │
│  + GetState(): CodecState                                               │
│  + GetConfig(): unique_ptr<ICodecConfig>                                │
│  + GetStats(): CodecStats                                               │
│  + IsSupported(config): bool                                            │
└─────────────────────────────┬───────────────────────────────────────────┘
                              │
              ┌───────────────┴───────────────┐
              ▼                               ▼
┌─────────────────────────┐     ┌─────────────────────────┐
│      IEncoder           │     │      IDecoder           │
├─────────────────────────┤     ├─────────────────────────┤
│ + SetConfig(config)     │     │ + SetConfig(config)     │
│ + Encode(frame): error  │     │ + Decode(frame): error  │
│ + EncodeBatch(): error  │     │ + DecodeBatch(): error  │
│ + Flush(): error        │     │ + Flush(): error        │
│ + RequestKeyframe()     │     │ + SetParameterSets()    │
│ + SetBitrate()          │     │ + GetSPS()/GetPPS()     │
│ + SetFramerate()        │     │ + SetPacketLossRate()   │
│ + SetCallback()         │     │ + NotifyPacketLost()    │
└───────────┬─────────────┘     └───────────┬─────────────┘
            │                               │
            ▼                               ▼
┌─────────────────────────┐     ┌─────────────────────────┐
│   OpusEncoder           │     │   OpusDecoder           │
│   H264Encoder          │     │   H264Decoder            │
│   (具体实现)            │     │   (具体实现)             │
└─────────────────────────┘     └─────────────────────────┘
```

---

## 9. 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v3.0 | 2026-03-10 | 初始版本，定义完整编解码接口 |
