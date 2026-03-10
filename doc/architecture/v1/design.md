# MiniRTC 架构设计文档 v1.0

**版本**: 1.0
**日期**: 2026-03-10
**状态**: 第1轮评审完成

---

## 1. 系统架构图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              MiniRTC Core                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐   │
│  │   会话管理       │◄──►│   媒体流水线     │◄──►│   时钟同步      │   │
│  │   SessionMgr    │    │   MediaPipeline │    │   ClockSync     │   │
│  └────────┬────────┘    └────────┬────────┘    └─────────────────┘   │
│           │                       │                                     │
│           ▼                       ▼                                     │
│  ┌─────────────────┐    ┌─────────────────────────────────────────┐   │
│  │   设备管理      │    │            Transport Layer              │   │
│  │   DeviceMgr     │    │  ┌─────────────┐   ┌────────────────┐  │   │
│  └────────┬────────┘    │  │RTPTransport │◄──►│ SRTPTransport  │  │   │
│           │             │  │  (默认)      │   │   (预留)       │  │   │
│           ▼             │  └──────┬──────┘   └────────────────┘  │   │
│  ┌─────────────────┐    │         │                                  │   │
│  │  IVideoCapture │    │         ▼                                  │   │
│  │  IAudioCapture │    │  ┌─────────────┐   ┌────────────────┐    │   │
│  └────────┬────────┘    │  │RTPModule   │◄──►│  RTCPModule    │    │   │
│           │           │  │            │   │  (QoS监控)     │    │   │
│           ▼           │  └─────────────┘   └────────────────┘    │   │
│  ┌─────────────────┐    └─────────────────────────────────────────┘   │
│  │  音视频播放     │                       ▲                           │
│  │  AVRenderer    │                       │                           │
│  └─────────────────┘                       │                           │
│           │                               │                           │
│           ▼                               │                           │
│  ┌─────────────────────────────────────────┴────────────────────────┐ │
│  │                        Codec Layer                                 │ │
│  │  ┌─────────────────┐              ┌─────────────────┐             │ │
│  │  │  AudioCodec     │              │  VideoCodec     │             │ │
│  │  │  (Opus Encoder/Decoder)       │  (openh264)     │             │ │
│  │  └─────────────────┘              └─────────────────┘             │ │
│  └───────────────────────────────────────────────────────────────────┘ │
│                                                                         │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐    │
│  │  JitterBuffer   │◄──►│    Stats        │◄──►│    glog         │    │
│  │  (音视频同步)    │    │  (统计分析)     │    │    (日志)       │    │
│  └─────────────────┘    └─────────────────┘    └─────────────────┘    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 模块清单

| 模块 | 描述 | 依赖 |
|------|------|------|
| SessionMgr | 会话生命周期、SDP/ICE | RTPTransport |
| MediaPipeline | 音视频数据流编排 | Codec, JitterBuffer |
| ClockSync | 时钟同步 | - |
| DeviceMgr | 音视频设备枚举 | 平台原生API |
| IVideoCapture | 视频采集抽象接口 | - |
| IAudioCapture | 音频采集抽象接口 | - |
| AVRenderer | 音视频播放 | - |
| AudioCodec | Opus编解码 | libopus |
| VideoCodec | H.264编解码 | openh264 |
| RTPTransport | RTP包收发 | - |
| RTCPModule | QoS监控 | RTP |
| JitterBuffer | 音视频同步 | - |
| Stats | 统计分析 | - |
| SRTPTransport | SRTP预留 | RTPTransport |

---

## 3. 核心接口定义 (Google风格)

### 3.1 基础接口模板

```cpp
namespace minirtc {

class Interface {
public:
    virtual ~Interface() = default;
    virtual void AddRef() = 0;
    virtual void Release() = 0;
};

}  // namespace minirtc
```

### 3.2 视频采集接口

```cpp
namespace minirtc {

enum class VideoPixelFormat {
    kI420,
    kNV12,
    kRGBA,
    kBGRA
};

struct VideoFrame {
    VideoPixelFormat format;
    int width;
    int height;
    int64_t timestamp_us;
    uint32_t timestamp_rtp;
    std::vector<uint8_t> data;
};

class VideoCaptureCallback {
public:
    virtual ~VideoCaptureCallback() = default;
    virtual void OnFrameCaptured(const VideoFrame& frame) = 0;
    virtual void OnCaptureError(int error_code, const std::string& error_msg) = 0;
};

class IVideoCapture : public Interface {
public:
    struct DeviceInfo {
        std::string device_id;
        std::string device_name;
        std::string unique_id;
    };
    
    struct CaptureParam {
        std::string device_id;
        int width = 640;
        int height = 480;
        int fps = 30;
        VideoPixelFormat format = VideoPixelFormat::kI420;
    };
    
    virtual bool GetDevices(std::vector<DeviceInfo>* devices) = 0;
    virtual bool Initialize(const CaptureParam& param) = 0;
    virtual bool StartCapture(VideoCaptureCallback* callback) = 0;
    virtual bool StopCapture() = 0;
    virtual bool IsCapturing() const = 0;
};

}  // namespace minirtc
```

### 3.3 传输层接口

```cpp
namespace minirtc {

class ITransport {
public:
    virtual int SendPacket(const MediaPacket& pkt) = 0;
    virtual void SetRemote(const SocketAddr& addr) = 0;
    // 预留SRTP接口
    virtual void SetCrypto(const CryptoConfig& config) = 0;
};

class RTPTransport : public ITransport {
    // 实现RTP收发
};

class SRTPTransport : public ITransport {
    // 预留：SRTP实现
};

}  // namespace minirtc
```

---

## 4. 线程模型

```
[采集线程] ──▶ [FrameQueue] ──▶ [编码线程池] ──▶ [RTP发送线程]
                     │
                     ▼
              [播放线程] ◀── [解码线程池] ◀── [RTP接收线程]
```

- 无锁队列：MPSC Ring Buffer
- 采集/播放线程分离
- RTP收发共用I/O线程

---

## 5. 技术选型

| 组件 | 选型 | 备注 |
|------|------|------|
| 视频编码 | openh264 | BSD许可 |
| 音频编码 | Opus | - |
| 单元测试 | Google Test | - |
| 日志 | glog | - |
| 传输 | RTP + RTCP | - |
| SRTP | 预留 | 架构预留 |

---

## 6. 评审意见（已采纳）

### 架构师C提出的补充
- [x] RTCP模块
- [x
- [x] Jitter Buffer] Stats统计模块
- [x] 时钟同步方案
- [x] 接口抽象设计
- [x] Transport分层

---

## 7. 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2026-03-10 | 第1轮评审完成 |
