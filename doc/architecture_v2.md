# MiniRTC 新阶段架构设计文档

**版本**: 1.0  
**日期**: 2026-03-11  
**架构师**: A  

---

## 1. 概述

本文档描述MiniRTC项目新阶段的架构设计，主要包括：
- Stream/Track抽象层
- 端到端测试框架
- Jitter Buffer模块
- 视频打包/组帧模块

---

## 2. 模块设计

### 2.1 模块结构总览

```
┌─────────────────────────────────────────────────────────────────┐
│                         Application                              │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                    StreamManager                          │   │
│  │  (管理多个Stream)                                         │   │
│  └─────────────────────────────────────────────────────────┘   │
│           │                              │                      │
│  ┌────────▼────────┐          ┌────────▼────────┐              │
│  │      Stream      │          │      Stream      │              │
│  │  (会话流，包含    │          │  (另一端)         │              │
│  │   多个Track)     │          │                  │              │
│  └────────┬────────┘          └────────┬────────┘              │
│           │                            │                        │
│  ┌────────▼────────┐          ┌────────▼────────┐              │
│  │      Track       │          │      Track       │              │
│  │ (音/视频轨道)    │          │ (音/视频轨道)    │              │
│  └────────┬────────┘          └────────┬────────┘              │
│           │                            │                        │
│  ┌────────▼────────┐          ┌────────▼────────┐              │
│  │  MediaSource     │          │    MediaSink     │              │
│  │ (采集/编码)       │          │ (解码/播放)       │              │
│  └────────┬────────┘          └────────┬────────┘              │
│           │                            │                        │
│  ┌────────▼────────┐          ┌────────▼────────┐              │
│  │    RTPTransport │◄────────►│   RTPTransport  │              │
│  └────────┬────────┘   Socket   └────────┬────────┘              │
│           │                            │                        │
│           └──────────┬─────────────────┘                        │
│                      ▼                                          │
│            ┌─────────────────┐                                 │
│            │  JitterBuffer    │                                 │
│            │  (透传模式)       │                                 │
│            └─────────────────┘                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 各模块职责

| 模块 | 职责 |
|------|------|
| StreamManager | 管理所有Stream生命周期，提供Stream创建/销毁接口 |
| Stream | 代表一个通话会话，管理多个Track的集合 |
| Track | 单一媒体轨道，管理音/视频的采集、编码、传输 |
| MediaSource | 媒体采集和编码 |
| MediaSink | 媒体解码和播放 |
| RTPTransport | RTP/RTCP数据收发 |
| JitterBuffer | 抖动缓冲（透传模式） |
| H264Packer | H.264 NALU打包为RTP |
| VideoAssembler | 组帧：拼接FU-A |

---

## 3. 类图/接口设计

### 3.1 核心接口定义

```cpp
// ============================================================================
// Media Type 定义
// ============================================================================

enum class MediaKind {
    kAudio = 1,
    kVideo = 2,
};

// ============================================================================
// ITrack 接口 - 单一音/视频轨道
// ============================================================================

class ITrack : public std::enable_shared_from_this<ITrack> {
public:
    using Ptr = std::shared_ptr<ITrack>;
    
    virtual ~ITrack() = default;
    
    // 获取轨道类型
    virtual MediaKind GetKind() const = 0;
    
    // 获取轨道ID
    virtual uint32_t GetId() const = 0;
    
    // 获取轨道名称
    virtual std::string GetName() const = 0;
    
    // 获取SSRC
    virtual uint32_t GetSsrc() const = 0;
    
    // 启动轨道
    virtual bool Start() = 0;
    
    // 停止轨道
    virtual void Stop() = 0;
    
    // 是否运行中
    virtual bool IsRunning() const = 0;
    
    // 发送RTP包
    virtual void SendRtpPacket(std::shared_ptr<RtpPacket> packet) = 0;
    
    // 接收RTP包
    virtual void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) = 0;
    
    // 获取统计信息
    virtual TrackStats GetStats() const = 0;
};

// ============================================================================
// IStream 接口 - 会话流
// ============================================================================

class IStream : public std::enable_shared_from_this<IStream> {
public:
    using Ptr = std::shared_ptr<IStream>;
    
    virtual ~IStream() = default;
    
    // 获取Stream ID
    virtual uint32_t GetId() const = 0;
    
    // 获取Stream名称
    virtual std::string GetName() const = 0;
    
    // 添加Track
    virtual bool AddTrack(ITrack::Ptr track) = 0;
    
    // 移除Track
    virtual bool RemoveTrack(uint32_t track_id) = 0;
    
    // 获取所有Track
    virtual std::vector<ITrack::Ptr> GetTracks() const = 0;
    
    // 按类型获取Track
    virtual ITrack::Ptr GetTrack(MediaKind kind) const = 0;
    
    // 启动Stream（启动所有Track）
    virtual bool Start() = 0;
    
    // 停止Stream（停止所有Track）
    virtual void Stop() = 0;
    
    // 是否运行中
    virtual bool IsRunning() const = 0;
    
    // 获取统计信息
    virtual StreamStats GetStats() const = 0;
};

// ============================================================================
// IStreamManager 接口 - Stream管理器
// ============================================================================

class IStreamManager {
public:
    using Ptr = std::shared_ptr<IStreamManager>;
    
    virtual ~IStreamManager() = default;
    
    // 创建Stream
    virtual IStream::Ptr CreateStream(const std::string& name) = 0;
    
    // 销毁Stream
    virtual bool DestroyStream(uint32_t stream_id) = 0;
    
    // 获取Stream
    virtual IStream::Ptr GetStream(uint32_t stream_id) const = 0;
    
    // 获取所有Stream
    virtual std::vector<IStream::Ptr> GetAllStreams() const = 0;
    
    // 启动所有Stream
    virtual bool StartAll() = 0;
    
    // 停止所有Stream
    virtual void StopAll() = 0;
};

// ============================================================================
// IJitterBuffer 接口 - 抖动缓冲（透传模式）
// ============================================================================

class IJitterBuffer {
public:
    using Ptr = std::shared_ptr<IJitterBuffer>;
    
    virtual ~IJitterBuffer() = default;
    
    // 初始化
    virtual bool Initialize(const JitterBufferConfig& config) = 0;
    
    // 停止
    virtual void Stop() = 0;
    
    // 添加包
    virtual void AddPacket(std::shared_ptr<RtpPacket> packet) = 0;
    
    // 获取包（透传：立即返回）
    virtual std::shared_ptr<RtpPacket> GetPacket(int timeout_ms) = 0;
    
    // 获取统计信息
    virtual JitterBufferStats GetStats() const = 0;
};

// ============================================================================
// IH264Packer 接口 - H.264打包
// ============================================================================

class IH264Packer {
public:
    using Ptr = std::shared_ptr<IH264Packer>;
    
    virtual ~IH264Packer() = default;
    
    // 打包单个NALU
    virtual std::vector<std::shared_ptr<RtpPacket>> PackNalu(
        const uint8_t* nalu_data, 
        size_t nalu_size,
        uint32_t timestamp,
        bool marker
    ) = 0;
    
    // 打包FU-A分片
    virtual std::shared_ptr<RtpPacket> PackFuA(
        const uint8_t* nalu_data,
        size_t nalu_size,
        size_t offset,
        size_t fragment_size,
        uint32_t timestamp,
        bool marker,
        uint8_t fu_header
    ) = 0;
};

// ============================================================================
// IVideoAssembler 接口 - 视频组帧
// ============================================================================

class IVideoAssembler {
public:
    using Ptr = std::shared_ptr<IVideoAssembler>;
    
    virtual ~IVideoAssembler() = default;
    
    // 输入RTP包
    virtual void AddPacket(std::shared_ptr<RtpPacket> packet) = 0;
    
    // 获取完整帧
    virtual std::shared_ptr<EncodedFrame> GetFrame(int timeout_ms) = 0;
    
    // 重置状态
    virtual void Reset() = 0;
};

// ============================================================================
// IEndToEndTest 接口 - 端到端测试
// ============================================================================

class IEndToEndTest {
public:
    using Ptr = std::shared_ptr<IEndToEndTest>;
    
    virtual ~IEndToEndTest() = default;
    
    // 初始化测试
    virtual bool Initialize(const EndToEndConfig& config) = 0;
    
    // 运行测试
    virtual bool Run() = 0;
    
    // 停止测试
    virtual void Stop() = 0;
    
    // 获取测试结果
    virtual TestResult GetResult() const = 0;
};
```

### 3.2 核心类实现

```cpp
// ============================================================================
// Track 实现
// ============================================================================

class Track : public ITrack {
public:
    Track(MediaKind kind, uint32_t id, const std::string& name);
    
    MediaKind GetKind() const override { return kind_; }
    uint32_t GetId() const override { return id_; }
    std::string GetName() const override { return name_; }
    uint32_t GetSsrc() const override { return ssrc_; }
    bool IsRunning() const override { return running_; }
    
    bool Start() override;
    void Stop() override;
    
    void SendRtpPacket(std::shared_ptr<RtpPacket> packet) override;
    void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) override;
    
    TrackStats GetStats() const override;
    
    // 设置编码器
    void SetEncoder(ICodec::Ptr encoder);
    
    // 设置解码器
    void SetDecoder(ICodec::Ptr decoder);
    
    // 设置传输层
    void SetTransport(std::shared_ptr<IRTPTransport> transport);
    
    // 设置JitterBuffer
    void SetJitterBuffer(IJitterBuffer::Ptr jitter_buffer);

private:
    MediaKind kind_;
    uint32_t id_;
    std::string name_;
    uint32_t ssrc_;
    
    std::atomic<bool> running_{false};
    
    ICodec::Ptr encoder_;
    ICodec::Ptr decoder_;
    std::shared_ptr<IRTPTransport> transport_;
    IJitterBuffer::Ptr jitter_buffer_;
    
    TrackStats stats_;
    mutable std::mutex mutex_;
};

// ============================================================================
// Stream 实现
// ============================================================================

class Stream : public IStream {
public:
    Stream(uint32_t id, const std::string& name);
    
    uint32_t GetId() const override { return id_; }
    std::string GetName() const override { return name_; }
    bool IsRunning() const override { return running_; }
    
    bool AddTrack(ITrack::Ptr track) override;
    bool RemoveTrack(uint32_t track_id) override;
    std::vector<ITrack::Ptr> GetTracks() const override;
    ITrack::Ptr GetTrack(MediaKind kind) const override;
    
    bool Start() override;
    void Stop() override;
    
    StreamStats GetStats() const override;
    
    // 设置本地传输配置
    void SetLocalTransport(const TransportConfig& config);
    
    // 设置远端传输地址
    void SetRemoteAddress(const NetworkAddress& addr);

private:
    uint32_t id_;
    std::string name_;
    std::map<uint32_t, ITrack::Ptr> tracks_;  // track_id -> Track
    std::atomic<bool> running_{false};
    
    TransportConfig local_config_;
    NetworkAddress remote_addr_;
    
    mutable std::mutex mutex_;
};

// ============================================================================
// StreamManager 实现
// ============================================================================

class StreamManager : public IStreamManager {
public:
    StreamManager();
    ~StreamManager();
    
    IStream::Ptr CreateStream(const std::string& name) override;
    bool DestroyStream(uint32_t stream_id) override;
    IStream::Ptr GetStream(uint32_t stream_id) const override;
    std::vector<IStream::Ptr> GetAllStreams() const override;
    
    bool StartAll() override;
    void StopAll() override;

private:
    std::map<uint32_t, IStream::Ptr> streams_;
    std::atomic<uint32_t> next_stream_id_{1};
    mutable std::mutex mutex_;
};
```

---

## 4. 线程模型

### 4.1 整体线程模型

```
┌─────────────────────────────────────────────────────────────────┐
│                        Main Thread                              │
│  ┌─────────────────┐                                           │
│  │  StreamManager  │  (创建/销毁Stream/Track)                    │
│  └────────┬────────┘                                           │
│           │                                                     │
│           ▼                                                     │
│  ┌─────────────────┐                                           │
│  │     Stream      │  (业务逻辑)                                 │
│  └────────┬────────┘                                           │
│           │                                                     │
├───────────┼─────────────────────────────────────────────────────┤
│           │                                                     │
│  ┌────────▼────────┐    ┌─────────────────┐                    │
│  │   Track Send    │    │  Track Receive  │                    │
│  │   (编码线程)     │    │   (解码线程)     │                    │
│  └────────┬────────┘    └────────┬────────┘                    │
│           │                      │                               │
│           ▼                      ▼                               │
│  ┌─────────────────┐    ┌─────────────────┐                    │
│  │ RTPTransport    │◄──►│ RTPTransport    │                    │
│  │ (发送线程)       │    │ (接收线程)        │                    │
│  └────────┬────────┘    └────────┬────────┘                    │
│           │                      │                               │
└───────────┼──────────────────────┼───────────────────────────────┘
            │                      │
            │              Socket   │
            └──────────────────────┘
```

### 4.2 线程职责

| 线程 | 职责 | 同步机制 |
|------|------|----------|
| Main Thread | Stream/Track管理，业务逻辑 | 无需同步 |
| Track Send | 采集→编码→打包→发送 | std::mutex |
| Track Receive | 接收→解包→解码→播放 | std::mutex |
| RTP Send | RTP包发送 | 内部队列 |
| RTP Receive | RTP包接收 | 回调+队列 |

### 4.3 数据流向

**发送方向:**
```
Capture → Encoder → H264Packer → RTPTransport → Socket
```

**接收方向:**
```
Socket → RTPTransport → JitterBuffer → VideoAssembler → Decoder → Render
```

---

## 5. 关键数据结构

### 5.1 Track 相关

```cpp
struct TrackStats {
    uint64_t packets_sent = 0;
    uint64_t packets_received = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t frames_sent = 0;
    uint64_t frames_received = 0;
    uint32_t bitrate_bps = 0;
    float packet_loss_rate = 0.0f;
};
```

### 5.2 Stream 相关

```cpp
struct StreamStats {
    uint32_t track_count = 0;
    uint64_t total_packets_sent = 0;
    uint64_t total_packets_received = 0;
    uint32_t duration_ms = 0;
};
```

### 5.3 JitterBuffer 配置

```cpp
struct JitterBufferConfig {
    MediaKind media_kind = MediaKind::kAudio;
    
    // 透传模式配置
    bool passthrough_mode = true;  // 透传模式，不做缓冲
    
    // 最小延迟（毫秒）
    uint32_t min_delay_ms = 0;
    
    // 最大延迟（毫秒）
    uint32_t max_delay_ms = 100;
    
    // 目标延迟（毫秒）
    uint32_t target_delay_ms = 0;
};

struct JitterBufferStats {
    uint64_t packets_received = 0;
    uint64_t packets_delivered = 0;
    uint64_t packets_lost = 0;
    uint32_t current_delay_ms = 0;
};
```

### 5.4 H.264 RTP 打包

```cpp
// NALU类型
enum class NalUnitType {
    kNonIdr = 1,
    kPartA = 2,
    kPartB = 3,
    kPartC = 4,
    kIdr = 5,
    kSEI = 6,
    kSPS = 7,
    kPPS = 8,
    kAccessUnitDelimiter = 9,
    kEndOfSequence = 10,
    kEndOfStream = 11,
    kFiller = 12,
    kSTAPA = 24,   // Single-time aggregation packet
    kSTAPB = 25,   // Single-time aggregation packet
    kMTAP16 = 26,  // Multi-time aggregation packet
    kMTAP24 = 27,  // Multi-time aggregation packet
    kFU_A = 28,    // Fragmentation unit
    kFU_B = 29,    // Fragmentation unit
};

// FU-A Header
struct FuAHeader {
    uint8_t forbidden_zero_bit : 1;
    uint8_t nal_ref_idc : 2;
    uint8_t nal_unit_type : 5;
    uint8_t start_bit : 1;
    uint8_t end_bit : 1;
    uint8_t reserved : 6;
};
```

### 5.5 端到端测试配置

```cpp
struct EndToEndConfig {
    // 本端配置
    NetworkAddress local_rtp_addr;
    NetworkAddress local_rtcp_addr;
    
    // 远端配置
    NetworkAddress remote_rtp_addr;
    NetworkAddress remote_rtcp_addr;
    
    // 测试参数
    uint32_t test_duration_ms = 10000;  // 测试时长
    uint32_t audio_payload_type = 111;  // Opus
    uint32_t video_payload_type = 96;   // H264
    
    // 测试模式
    bool enable_audio = true;
    bool enable_video = true;
    
    // 视频参数
    uint32_t video_width = 640;
    uint32_t video_height = 480;
    uint32_t video_fps = 30;
    
    // 音频参数
    uint32_t audio_sample_rate = 48000;
    uint32_t audio_channels = 1;
};

struct TestResult {
    bool success = false;
    uint64_t audio_packets_sent = 0;
    uint64_t audio_packets_received = 0;
    uint64_t video_packets_sent = 0;
    uint64_t video_packets_received = 0;
    float audio_packet_loss = 0.0f;
    float video_packet_loss = 0.0f;
    uint32_t avg_latency_ms = 0;
    std::string error_message;
};
```

---

## 6. 目录结构设计

```
include/minirtc/
├── stream/                    # 新增：Stream/Track模块
│   ├── stream_manager.h
│   ├── stream.h
│   ├── track.h
│   └── stream_types.h
├── jitter_buffer/            # 新增：Jitter Buffer模块
│   ├── jitter_buffer.h
│   └── jitter_buffer_config.h
├── rtp_packer/               # 新增：RTP打包模块
│   ├── h264_packer.h
│   ├── video_assembler.h
│   └── rtp_packer_types.h
└── test/                     # 新增：端到端测试模块
    ├── e2e_test.h
    └── e2e_test_types.h

src/
├── stream/                   # Stream/Track实现
│   ├── stream_manager.cc
│   ├── stream.cc
│   └── track.cc
├── jitter_buffer/            # Jitter Buffer实现
│   └── jitter_buffer.cc
├── rtp_packer/               # RTP打包实现
│   ├── h264_packer.cc
│   └── video_assembler.cc
└── test/                     # 端到端测试实现
    └── e2e_test.cc
```

---

## 7. API使用示例

### 7.1 创建通话

```cpp
// 1. 创建StreamManager
auto stream_manager = std::make_shared<StreamManager>();

// 2. 创建Stream
auto stream = stream_manager->CreateStream("call-001");

// 3. 添加Audio Track
auto audio_track = std::make_shared<Track>(MediaKind::kAudio, 1, "audio");
audio_track->SetEncoder(CreateOpusEncoder());
audio_track->SetTransport(rtp_transport);
stream->AddTrack(audio_track);

// 4. 添加Video Track
auto video_track = std::make_shared<Track>(MediaKind::kVideo, 2, "video");
video_track->SetEncoder(CreateH264Encoder());
video_track->SetTransport(rtp_transport);
stream->AddTrack(video_track);

// 5. 设置远端地址
stream->SetRemoteAddress(NetworkAddress("192.168.1.100", 5000));

// 6. 启动通话
stream->Start();
```

### 7.2 端到端测试

```cpp
// 配置测试
EndToEndConfig config;
config.local_rtp_addr = {"0.0.0.0", 5000};
config.remote_rtp_addr = {"127.0.0.1", 6000};
config.test_duration_ms = 10000;
config.enable_video = true;

// 创建测试
auto test = std::make_shared<EndToEndTest>();
test->Initialize(config);

// 运行测试
test->Run();

// 获取结果
auto result = test->GetResult();
if (result.success) {
    printf("Test passed! Video packets: %lu/%lu\n",
           result.video_packets_received,
           result.video_packets_sent);
}
```

---

## 8. 实现优先级

1. **Phase 1: Stream/Track抽象**
   - StreamManager, Stream, Track类
   - 简化通话创建流程

2. **Phase 2: Jitter Buffer**
   - 透传模式实现
   - 音视频支持

3. **Phase 3: H.264 RTP打包**
   - NALU打包
   - FU-A分片组帧

4. **Phase 4: 端到端测试框架**
   - 双端Socket通信
   - RTP/RTCP传输
   - 1对1通话测试

---

## 9. 总结

本架构设计：
- **简化通话创建**: 通过Stream/Track抽象，一行代码创建通话
- **模块化设计**: 各模块职责清晰，易于扩展
- **透传模式**: Jitter Buffer实现最简单的透传，支持音视频
- **完整H.264支持**: 打包+组帧完整实现
- **可测试性**: 端到端测试框架支持1对1通话测试
