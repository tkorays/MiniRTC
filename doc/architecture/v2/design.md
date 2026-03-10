# MiniRTC 架构设计文档 v2.0

**版本**: 2.0
**日期**: 2026-03-10
**状态**: 第2轮评审

---

## 1. 系统架构图

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              MiniRTC Core v2                                     │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐   │
│  │  SessionMgr │◄──►│MediaPipeline│◄──►│  ClockSync  │◄──►│  DeviceMgr  │   │
│  └─────────────┘    └──────┬──────┘    └─────────────┘    └─────────────┘   │
│         │                  │                                                      │
│         ▼                  ▼                                                      │
│  ┌─────────────────────────────────────────────────────────────────────────┐    │
│  │                        媒体处理层                                        │    │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │    │
│  │  │IVideoCap │  │IAudioCap │  │IVideoRdr │  │IAudioPln │  │AudioCodec│  │    │
│  │  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘  │    │
│  │       │            │            │            │            │        │    │
│  │  ┌────▼────────────▼────────────▼────────────▼────────────▼────┐   │    │
│  │  │                    MediaFrameBus (媒体帧总线)                 │   │    │
│  │  └────────────────────────────┬────────────────────────────────┘   │    │
│  │                               │                                      │    │
│  │  ┌────────────┐  ┌────────────┐  │  ┌────────────┐  ┌────────────┐  │    │
│  │  │VideoCodec  │  │VideoCodec  │  │  │VideoCodec  │  │VideoCodec  │  │    │
│  │  │  (H.264)   │  │  (H.264)   │  │  │  (H.264)   │  │  (H.264)   │  │    │
│  │  │  Encoder   │  │  Decoder   │  │  │  Encoder   │  │  Decoder   │  │    │
│  │  └─────┬──────┘  └─────┬──────┘  │  └─────┬──────┘  └─────┬──────┘  │    │
│  │        │               │         │        │               │        │    │
│  │        └───────────────┼─────────┼────────┴───────────────┘        │    │
│  │                        ▼                                           │    │
│  │  ┌──────────────────────────────────────────────────────────────────┐ │    │
│  │  │              抗丢包模块 (丢包恢复)                                │ │    │
│  │  │  ┌────────────────────┐       ┌────────────────────┐            │ │    │
│  │  │  │   NACK Module     │◄──────►│   FEC Module      │            │ │    │
│  │  │  │  (丢包重传请求)    │       │   (前向纠错)       │            │ │    │
│  │  │  └─────────┬──────────┘       └─────────┬─────────┘            │ │    │
│  │  │            │                            │                       │ │    │
│  │  │  ┌─────────▼────────────────────────────▼─────────┐            │ │    │
│  │  │  │            PacketCache (包缓存)                │            │ │    │
│  │  │  └────────────────────────────────────────────────┘            │ │    │
│  │  └──────────────────────────────────────────────────────────────────┘ │    │
│  └─────────────────────────────────────────────────────────────────────────┘    │
│                                        │                                          │
│  ┌─────────────────────────────────────▼─────────────────────────────────────┐   │
│  │                        传输层                                              │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │   │
│  │  │ RTPTransport │  │ RTCPModule   │  │SRTPTransport │  │   Stats      │  │   │
│  │  │   (RTP)      │  │   (RTCP)     │  │   (预留)     │  │   (统计)     │  │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘  │   │
│  │         │                 │                │                │          │   │
│  │         └─────────────────┼────────────────┴────────────────┘          │   │
│  │                           ▼                                            │   │
│  │  ┌──────────────────────────────────────────────────────────────────┐  │   │
│  │  │                    NetworkInterface (网络接口)                   │  │   │
│  │  │                      ┌──────────────┐                            │  │   │
│  │  │                      │  ICE (预留)  │                            │  │   │
│  │  │                      ├──────────────┤                            │  │   │
│  │  │                      │ DTLS (预留)  │                            │  │   │
│  │  │                      ├──────────────┤                            │  │   │
│  │  │                      │ SDP (预留)   │                            │  │   │
│  │  │                      └──────────────┘                            │  │   │
│  │  └──────────────────────────────────────────────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│                                        │                                          │
│  ┌─────────────────────────────────────▼─────────────────────────────────────┐   │
│  │                        应用层                                              │   │
│  │  ┌──────────────────────────────────────────────────────────────────┐   │   │
│  │  │                    Audio Processing (音频处理)                   │   │   │
│  │  │  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐      │   │   │
│  │  │  │  AEC (预留)   │  │    ANS        │  │    AGC        │      │   │   │
│  │  │  │ (回声消除)    │  │  (噪声抑制)   │  │  (自动增益)   │      │   │   │
│  │  │  └────────────────┘  └────────────────┘  └────────────────┘      │   │   │
│  │  └──────────────────────────────────────────────────────────────────┘   │   │
│  │  ┌──────────────────────────────────────────────────────────────────┐   │   │
│  │  │                    Video Processing (视频处理)                   │   │   │
│  │  │  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐      │   │   │
│  │  │  │    Deinterlace │  │   ColorSpace   │  │   Resize      │      │   │   │
│  │  │  │   (去隔行)     │  │   (色彩空间)   │  │   (缩放)      │      │   │   │
│  │  │  └────────────────┘  └────────────────┘  └────────────────┘      │   │   │
│  │  └──────────────────────────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 模块清单 (v2.0)

| # | 模块 | 描述 | 状态 |
|---|------|------|------|
| 1 | SessionMgr | 会话管理 | 已有 |
| 2 | MediaPipeline | 媒体流水线 | 已有 |
| 3 | ClockSync | 时钟同步 | 已有 |
| 4 | DeviceMgr | 设备管理 | 已有 |
| 5 | IVideoCapture | 视频采集 | 已有 |
| 6 | IAudioCapture | 音频采集 | 已有 |
| 7 | IVideoRenderer | 视频播放 | 已有 |
| 8 | IAudioPlayer | 音频播放 | 已有 |
| 9 | AudioCodec | Opus编解码 | 已有 |
| 10 | VideoCodec | H.264编解码 | 已有 |
| 11 | RTPTransport | RTP传输 | 已有 |
| 12 | RTCPModule | RTCP传输 | 已有 |
| 13 | JitterBuffer | 抖动缓冲 | 已有 |
| 14 | Stats | 统计模块 | 已有 |
| 15 | SRTPTransport | SRTP预留 | 预留 |
| 16 | **NACK Module** | 丢包重传 | **新增** |
| 17 | **FEC Module** | 前向纠错 | **新增** |
| 18 | ICE | NAT穿透 | 预留 |
| 19 | DTLS | 密钥交换 | 预留 |
| 20 | SDP | 信令协商 | 预留 |
| 21 | AEC | 回声消除 | 预留 |

---

## 3. NACK 模块详细设计

### 3.1 NACK 工作流程

```
RTP包接收 -> 序列号检测 -> 丢包判断 -> NACK生成
                                                  │
       ┌─────────────────────────────────────────┘
       ▼
NACK发送(RTCP) -> 重传请求(RTX) -> 重传包接收 -> 包插入JitterBuffer
```

### 3.2 NACK 核心接口

```cpp
namespace minirtc {

enum class NackStatus {
  kOk,
  kTooManyRetries,
  kTtlExpired,
};

struct NackPacketInfo {
  uint16_t seq_num;
  uint16_t send_time;
  uint8_t retries;
  bool at_risk;
};

struct NackConfig {
  bool enable_nack = true;
  bool enable_rtx = true;
  int max_retransmissions = 3;
  int rtt_estimate_ms = 100;
  int nack_timeout_ms = 100;
  int max_nack_list_size = 250;
};

class NackModule {
 public:
  using OnNackRequestCallback = std::function<void(const std::vector<uint16_t>& seq_nums)>;
  
  virtual ~NackModule() = default;
  
  virtual void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) = 0;
  virtual void OnRtxPacketReceived(std::shared_ptr<RtpPacket> packet) = 0;
  virtual std::vector<uint16_t> GetNackList(int64_t now_ms) = 0;
  virtual void SetConfig(const NackConfig& config) = 0;
};

}  // namespace minirtc
```

---

## 4. FEC 模块详细设计

### 4.1 FEC 工作流程

```
发送端：原始RTP包 -> FEC编码 -> 生成冗余包 -> 混合发送
接收端：RTP包 + 冗余包 -> FEC解码 -> 恢复丢失包 -> 送入JitterBuffer
```

### 4.2 FEC 核心接口

```cpp
namespace minirtc {

enum class FecType {
  kUlpFec,    // Unequal Level Protection FEC
  kXorFec,    // Simple XOR FEC
};

struct FecConfig {
  bool enable_fec = true;
  FecType fec_type = FecType::kUlpFec;
  int media_payload_type = 96;
  int fec_payload_type = 97;
  int fec_percentage = 10;  // 冗余比例
  int max_fec_frames = 8;
};

class FecModule {
 public:
  virtual ~FecModule() = default;
  
  virtual void Encode(const std::vector<std::shared_ptr<RtpPacket>>& packets,
                     std::shared_ptr<RtpPacket>* fec_packet) = 0;
  virtual bool Decode(const std::vector<std::shared_ptr<RtpPacket>>& packets,
                     std::shared_ptr<RtpPacket>* recovered_packet) = 0;
  virtual void SetConfig(const FecConfig& config) = 0;
};

}  // namespace minirtc
```

---

## 5. 模块依赖关系

| 源模块 | 目标模块 | 依赖关系 |
|--------|----------|----------|
| SessionMgr | MediaPipeline | 强依赖 |
| SessionMgr | ClockSync | 强依赖 |
| MediaPipeline | DeviceMgr | 强依赖 |
| MediaPipeline | JitterBuffer | 强依赖 |
| MediaPipeline | NACK | 强依赖 |
| MediaPipeline | FEC | 强依赖 |
| MediaPipeline | RTPTransport | 强依赖 |
| JitterBuffer | NACK | 强依赖 |
| JitterBuffer | FEC | 强依赖 |
| RTPTransport | RTCPModule | 强依赖 |
| RTCPModule | Stats | 强依赖 |
| RTPTransport | ICE/DTLS/SDP | 预留 |

---

## 6. 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2026-03-10 | 第1轮评审完成 |
| v2.0 | 2026-03-10 | 添加NACK/FEC模块，预留ICE/DTLS/SDP/AEC |
