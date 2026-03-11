# MiniRTC 新阶段架构设计文档 (架构师B版本)

**版本**: 2.0  
**日期**: 2026-03-11  
**架构师**: B  

---

## 1. 概述

本文档是MiniRTC项目新阶段的架构设计，由架构师B完成。与架构师A的设计相比，本版本更强调：
- **实践导向**: 每个模块都有具体的实现路径
- **性能优化**: 关注内存分配、零拷贝等性能敏感点
- **测试优先**: 端到端测试框架先行，确保设计可验证

---

## 2. 核心架构

### 2.1 分层架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        应用层 (Application)                      │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                    CallController                         │   │
│  │  (通话控制器，统一管理Stream生命周期)                       │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│                      媒体层 (Media Layer)                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ MediaCapture │  │ MediaEncoder │  │ MediaRender │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
├─────────────────────────────────────────────────────────────────┤
│                     Stream/Track 层                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │   IStream    │  │    ITrack    │  │TrackFactory │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
├─────────────────────────────────────────────────────────────────┤
│                      传输层 (Transport)                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ RTPTransport │  │ RtcpSession  │  │   Dtls      │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
├─────────────────────────────────────────────────────────────────┤
│                      抖动缓冲层 (JitterBuffer)                    │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │  PassThrough │  │   Adaptive   │  │VideoAssembler│          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
├─────────────────────────────────────────────────────────────────┤
│                     打包层 (Packing)                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │  H264Packer  │  │   OpusPack   │  │  RtpHeader  │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 数据流架构

```
发送路径:
┌────────────┐   ┌────────────┐   ┌────────────┐   ┌────────────┐
│   采集     │──▶│   编码     │──▶│   RTP打包   │──▶│   发送     │
│ (Capture)  │   │ (Encoder)  │   │ (Packer)   │   │(Transport) │
└────────────┘   └────────────┘   └────────────┘   └────────────┘

接收路径:
┌────────────┐   ┌────────────┐   ┌────────────┐   ┌────────────┐
│   接收     │──▶│ JitterBuffer│──▶│  组帧      │──▶│   解码     │
│(Transport) │   │ (Passthrough)│ │(Assembler) │   │ (Decoder) │
└────────────┘   └────────────┘   └────────────┘   └────────────┘
```

---

## 3. Stream/Track 抽象设计

### 3.1 设计理念

**为什么需要Stream/Track?**
- **简化API**: 用户只需要创建Stream，Track自动管理
- **生命周期统一**: Stream启动时自动启动所有Track
- **资源管理**: Stream销毁时自动释放所有Track资源

### 3.2 核心接口

```cpp
// ============================================================================
// MediaKind - 媒体类型
// ============================================================================
enum class MediaKind : uint8_t {
    kAudio = 0,
    kVideo = 1,
    kData  = 2,  // 未来扩展: 屏幕共享、白板等
};

// ============================================================================
// TrackState - Track状态机
// ============================================================================
enum class TrackState : uint8_t {
    kInit = 0,       // 初始化
    kStarting,       // 启动中
    kRunning,        // 运行中
    kStopping,       // 停止中
    kStopped,        // 已停止
    kFailed,         // 失败
};

// ============================================================================
// ITrack - 单一媒体轨道
// ============================================================================
class ITrack : public std::enable_shared_from_this<ITrack> {
public:
    using Ptr = std::shared_ptr<ITrack>;
    using WeakPtr = std::weak_ptr<ITrack>;
    
    virtual ~ITrack() = default;
    
    // ---------- 属性 ----------
    virtual MediaKind GetKind() const = 0;
    virtual uint32_t GetId() const = 0;
    virtual std::string GetName() const = 0;
    virtual uint32_t GetSsrc() const = 0;
    
    // ---------- 状态 ----------
    virtual TrackState GetState() const = 0;
    virtual bool IsRunning() const = 0;
    virtual bool IsMuted() const = 0;
    virtual bool IsEnabled() const = 0;
    
    // ---------- 生命周期 ----------
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    
    // ---------- 媒体处理 ----------
    // 发送RTP包 (内部调用)
    virtual void SendRtpPacket(std::shared_ptr<RtpPacket> packet) = 0;
    
    // 接收RTP包 (由Transport调用)
    virtual void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) = 0;
    
    // 编码器输出 (由Encoder调用)
    virtual void OnEncodedFrame(std::shared_ptr<EncodedFrame> frame) = 0;
    
    // 解码器输出 (输出到渲染器)
    virtual std::shared_ptr<DecodedFrame> GetDecodedFrame(int timeout_ms) = 0;
    
    // ---------- 配置 ----------
    virtual void SetSsrc(uint32_t ssrc) = 0;
    virtual void SetEncoder(std::shared_ptr<IMediaEncoder> encoder) = 0;
    virtual void SetDecoder(std::shared_ptr<IMediaDecoder> decoder) = 0;
    virtual void SetTransport(std::shared_ptr<IRTPTransport> transport) = 0;
    virtual void SetPacker(std::shared_ptr<IRtpPacker> packer) = 0;
    
    // ---------- 统计 ----------
    virtual TrackStats GetStats() const = 0;
    virtual void ResetStats() = 0;
};

// ============================================================================
// IStream - 会话流
// ============================================================================
class IStream : public std::enable_shared_from_this<IStream> {
public:
    using Ptr = std::shared_ptr<IStream>;
    using WeakPtr = std::weak_ptr<IStream>;
    
    virtual ~IStream() = default;
    
    // ---------- 属性 ----------
    virtual uint32_t GetId() const = 0;
    virtual std::string GetName() const = 0;
    virtual uint64_t GetCreateTime() const = 0;
    
    // ---------- Track管理 ----------
    // 添加Track (自动分配ID)
    virtual ITrack::Ptr AddTrack(MediaKind kind, const std::string& name) = 0;
    
    // 添加已有Track
    virtual bool AddTrack(ITrack::Ptr track) = 0;
    
    // 移除Track
    virtual bool RemoveTrack(uint32_t track_id) = 0;
    
    // 获取Track
    virtual ITrack::Ptr GetTrack(uint32_t track_id) const = 0;
    virtual ITrack::Ptr GetTrack(MediaKind kind) const = 0;
    
    // 获取所有Track
    virtual std::vector<ITrack::Ptr> GetTracks() const = 0;
    virtual size_t GetTrackCount() const = 0;
    
    // ---------- 状态 ----------
    virtual bool IsRunning() const = 0;
    virtual bool HasAudio() const = 0;
    virtual bool HasVideo() const = 0;
    
    // ---------- 生命周期 ----------
    // 启动Stream (启动所有Track)
    virtual bool Start() = 0;
    
    // 停止Stream (停止所有Track)
    virtual void Stop() = 0;
    
    // ---------- 传输配置 ----------
    virtual void SetLocalTransportConfig(const TransportConfig& config) = 0;
    virtual void SetRemoteAddress(const SocketAddress& addr) = 0;
    virtual TransportConfig GetLocalTransportConfig() const = 0;
    virtual SocketAddress GetRemoteAddress() const = 0;
    
    // ---------- 统计 ----------
    virtual StreamStats GetStats() const = 0;
};

// ============================================================================
// IStreamManager - Stream管理器
// ============================================================================
class IStreamManager {
public:
    using Ptr = std::shared_ptr<IStreamManager>;
    
    virtual ~IStreamManager() = default;
    
    // 创建Stream (简化API)
    virtual IStream::Ptr CreateStream() = 0;
    virtual IStream::Ptr CreateStream(const std::string& name) = 0;
    
    // 销毁Stream
    virtual bool DestroyStream(uint32_t stream_id) = 0;
    virtual bool DestroyStream(IStream::Ptr stream) = 0;
    
    // 获取Stream
    virtual IStream::Ptr GetStream(uint32_t stream_id) const = 0;
    
    // 遍历所有Stream
    virtual void ForEachStream(std::function<void(IStream::Ptr)> callback) = 0;
    
    // 全局控制
    virtual bool StartAll() = 0;
    virtual void StopAll() = 0;
};
```

### 3.3 简化通话创建流程

**目标**: 一行代码创建通话

```cpp
// ============================================================================
// 简化API设计
// ============================================================================

class CallBuilder {
public:
    using Ptr = std::shared_ptr<CallBuilder>;
    
    // 链式配置
    static Ptr Create() { return std::make_shared<CallBuilder>(); }
    
    CallBuilder& SetName(const std::string& name) { 
        name_ = name; 
        return *this; 
    }
    
    CallBuilder& EnableAudio(bool enable = true) {
        enable_audio_ = enable;
        return *this;
    }
    
    CallBuilder& EnableVideo(bool enable = true) {
        enable_video_ = enable;
        return *this;
    }
    
    CallBuilder& SetVideoResolution(uint32_t width, uint32_t height) {
        video_width_ = width;
        video_height_ = height;
        return *this;
    }
    
    CallBuilder& SetRemoteAddress(const std::string& ip, uint16_t port) {
        remote_addr_ = SocketAddress(ip, port);
        return *this;
    }
    
    CallBuilder& SetLocalPort(uint16_t rtp_port) {
        local_rtp_port_ = rtp_port;
        return *this;
    }
    
    // 一键创建通话
    IStream::Ptr Build() {
        // 1. 创建StreamManager (单例)
        auto manager = StreamManager::Instance();
        
        // 2. 创建Stream
        auto stream = manager->CreateStream(name_);
        
        // 3. 配置传输
        TransportConfig config;
        config.local_rtp_port = local_rtp_port_;
        stream->SetLocalTransportConfig(config);
        stream->SetRemoteAddress(remote_addr_);
        
        // 4. 自动添加Track
        if (enable_audio_) {
            auto audio = stream->AddTrack(MediaKind::kAudio, "audio");
            audio->SetEncoder(CreateOpusEncoder());
            audio->SetPacker(CreateOpusPacker());
        }
        
        if (enable_video_) {
            auto video = stream->AddTrack(MediaKind::kVideo, "video");
            video->SetEncoder(CreateH264Encoder(video_width_, video_height_));
            video->SetPacker(CreateH264Packer());
        }
        
        return stream;
    }
    
private:
    std::string name_ = "call-" + GenerateId();
    bool enable_audio_ = true;
    bool enable_video_ = true;
    uint32_t video_width_ = 640;
    uint32_t video_height_ = 480;
    uint16_t local_rtp_port_ = 5000;
    SocketAddress remote_addr_;
};

// ============================================================================
// 使用示例
// ============================================================================

// 传统方式 (仍然支持)
auto manager = StreamManager::Instance();
auto stream = manager->CreateStream("call-001");
auto audio = stream->AddTrack(MediaKind::kAudio, "audio");
auto video = stream->AddTrack(MediaKind::kVideo, "video");
stream->SetRemoteAddress({"192.168.1.100", 5000});
stream->Start();

// 简化方式 (一行通话)
auto call = CallBuilder::Create()
    ->SetName("call-001")
    ->EnableAudio(true)
    ->EnableVideo(true)
    ->SetVideoResolution(1280, 720)
    ->SetRemoteAddress("192.168.1.100", 5000)
    ->Build();

call->Start();
```

---

## 4. Jitter Buffer 设计 (透传模式)

### 4.1 设计目标

**透传模式 (Passthrough Mode)**:
- 零延迟: RTP包到达后立即传递给上层
- 零缓冲: 不做任何排序或重传
- 适用于: 实时交互场景 (VoIP、视频通话)

### 4.2 核心接口

```cpp
// ============================================================================
// JitterBufferConfig - 配置
// ============================================================================
struct JitterBufferConfig {
    MediaKind media_kind = MediaKind::kAudio;
    
    // 模式选择
    enum class Mode : uint8_t {
        kPassthrough = 0,  // 透传模式 (零延迟)
        kAdaptive = 1,     // 自适应模式 (可变速率)
        kFixed = 2,        // 固定延迟模式
    };
    Mode mode = Mode::kPassthrough;
    
    // 透传模式配置 (无参数)
    
    // 自适应模式配置
    uint32_t min_delay_ms = 20;
    uint32_t max_delay_ms = 200;
    uint32_t default_delay_ms = 50;
    
    // 固定模式配置
    uint32_t fixed_delay_ms = 50;
    
    // 能力
    bool enable_rtx = false;       // 重传请求
    bool enable_fec = false;      // 前向纠错
};

// ============================================================================
// IJitterBuffer - 接口
// ============================================================================
class IJitterBuffer : public std::enable_shared_from_this<IJitterBuffer> {
public:
    using Ptr = std::shared_ptr<IJitterBuffer>;
    
    virtual ~IJitterBuffer() = default;
    
    // ---------- 生命周期 ----------
    virtual bool Initialize(const JitterBufferConfig& config) = 0;
    virtual void Stop() = 0;
    virtual void Reset() = 0;
    
    // ---------- 操作 ----------
    // 添加RTP包 (由Transport调用)
    virtual void AddPacket(std::shared_ptr<RtpPacket> packet) = 0;
    
    // 获取RTP包 (透传: 立即返回)
    virtual std::shared_ptr<RtpPacket> GetPacket(int timeout_ms) = 0;
    
    // 尝试获取包 (非阻塞)
    virtual std::shared_ptr<RtpPacket> TryGetPacket() = 0;
    
    // ---------- 统计 ----------
    virtual JitterBufferStats GetStats() const = 0;
    
    // ---------- 配置 ----------
    virtual void SetMode(JitterBufferConfig::Mode mode) = 0;
    virtual void SetDelay(uint32_t delay_ms) = 0;
};

// ============================================================================
// JitterBufferStats - 统计
// ============================================================================
struct JitterBufferStats {
    // 计数
    uint64_t packets_received = 0;
    uint64_t packets_delivered = 0;
    uint64_t packets_dropped = 0;
    uint64_t packets_reordered = 0;
    
    // 延迟
    uint32_t current_delay_ms = 0;
    uint32_t avg_delay_ms = 0;
    uint32_t max_delay_ms = 0;
    uint32_t min_delay_ms = 0;
    
    // 抖动
    int64_t jitter_us = 0;
    
    // 丢包
    uint64_t packets_lost = 0;
    float packet_loss_rate = 0.0f;
};
```

### 4.3 透传模式实现

```cpp
// ============================================================================
// PassThroughJitterBuffer - 透传模式实现
// ============================================================================
class PassThroughJitterBuffer : public IJitterBuffer {
public:
    PassThroughJitterBuffer();
    ~PassThroughJitterBuffer() override;
    
    bool Initialize(const JitterBufferConfig& config) override;
    void Stop() override;
    void Reset() override;
    
    void AddPacket(std::shared_ptr<RtpPacket> packet) override;
    std::shared_ptr<RtpPacket> GetPacket(int timeout_ms) override;
    std::shared_ptr<RtpPacket> TryGetPacket() override;
    
    JitterBufferStats GetStats() const override;
    
    void SetMode(JitterBufferConfig::Mode mode) override;
    void SetDelay(uint32_t delay_ms) override;

private:
    JitterBufferConfig config_;
    
    // 接收队列 (单生产者单消费者，可以使用lock-free)
    moodycamel::ConcurrentQueue<std::shared_ptr<RtpPacket>> recv_queue_;
    
    // 统计
    std::atomic<uint64_t> packets_received_{0};
    std::atomic<uint64_t> packets_delivered_{0};
    std::atomic<uint64_t> packets_dropped_{0};
    
    // 运行状态
    std::atomic<bool> running_{false};
    
    mutable std::mutex mutex_;
};

// 实现要点:
// 1. 使用lock-free queue实现无锁队列
// 2. GetPacket(timeout_ms=0) 立即返回，timeout_ms<0 阻塞
// 3. 不做任何排序或重传，真正透传
// 4. 统计仅做计数，不影响主流程
```

### 4.4 线程模型

```
透传模式线程模型:

┌─────────────────┐         ┌─────────────────┐
│ RTPTransport    │         │    Decoder      │
│   (接收线程)     │         │   (消费线程)     │
└────────┬────────┘         └────────┬────────┘
         │                            ▲
         │ AddPacket()                 │ GetPacket()
         ▼                            │
┌─────────────────────────────────────────┐
│       PassThroughJitterBuffer           │
│  ┌───────────────────────────────────┐  │
│  │   Lock-Free Queue (SPSC)         │  │
│  │   - 无锁入队                       │  │
│  │   - 无锁出队                       │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

---

## 5. H.264 RTP 打包设计

### 5.1 NALU打包策略

**打包规则 (RFC 6184)**:
1. **单一NALU**: NALU <= MTU (通常1400字节)，直接打包
2. **FU-A分片**: NALU > MTU，分片为多个FU-A

### 5.2 核心接口

```cpp
// ============================================================================
// H264NaluType - NALU类型
// ============================================================================
enum class H264NaluType : uint8_t {
    kNonIdr = 1,           // 非IDR Slice
    kPartA = 2,            // Slice Partition A
    kPartB = 3,            // Slice Partition B  
    kPartC = 4,            // Slice Partition C
    kIdr = 5,              // IDR Slice
    kSEI = 6,              // Supplemental Enhancement Information
    kSPS = 7,              // Sequence Parameter Set
    kPPS = 8,              // Picture Parameter Set
    kAccessUnitDelimiter = 9,
    kEndOfSequence = 10,
    kEndOfStream = 11,
    kFillerData = 12,
    
    // 聚合包
    kSTAPA = 24,           // Single-Time Aggregation Packet A
    kSTAPB = 25,           // Single-Time Aggregation Packet B
    kMTAP16 = 26,          // Multi-Time Aggregation Packet 16
    kMTAP24 = 27,          // Multi-Time Aggregation Packet 24
    
    // 分片
    kFUA = 28,             // Fragmentation Unit A
    kFUB = 29,             // Fragmentation Unit B
};

// ============================================================================
// IH264Packer - H.264打包接口
// ============================================================================
class IH264Packer : public std::enable_shared_from_this<IH264Packer> {
public:
    using Ptr = std::shared_ptr<IH264Packer>;
    
    virtual ~IH264Packer() = default;
    
    // ---------- 配置 ----------
    virtual void SetMaxPacketSize(size_t size) = 0;
    virtual void SetPayloadType(uint8_t pt) = 0;
    
    // ---------- 打包 ----------
    // 打包单个NALU (自动选择单包或FU-A)
    virtual std::vector<std::shared_ptr<RtpPacket>> PackNalu(
        const uint8_t* nalu_data,
        size_t nalu_size,
        uint32_t timestamp,
        bool marker
    ) = 0;
    
    // 打包FU-A (分片)
    virtual std::shared_ptr<RtpPacket> PackFuA(
        const uint8_t* nalu_data,
        size_t nalu_size,
        size_t offset,
        uint32_t timestamp,
        bool marker
    ) = 0;
    
    // 打包STAP-A (聚合)
    virtual std::shared_ptr<RtpPacket> PackStapA(
        const std::vector<std::pair<const uint8_t*, size_t>>& nalus,
        uint32_t timestamp
    ) = 0;
    
    // ---------- 统计 ----------
    virtual PackerStats GetStats() const = 0;
};

// ============================================================================
// 打包结果
// ============================================================================
struct PackerStats {
    uint64_t nalus_packed = 0;
    uint64_t single_nalus = 0;      // 单一NALU包
    uint64_t fragmented_nalus = 0;  // 分片的NALU
    uint64_t fu_a_packets = 0;      // FU-A包数量
    uint64_t stap_packets = 0;      // 聚合包数量
    uint64_t bytes_packed = 0;
};
```

### 5.3 FU-A分片实现

```cpp
// ============================================================================
// H264Packer 实现
// ============================================================================
class H264Packer : public IH264Packer {
public:
    H264Packer();
    ~H264Packer() override;
    
    void SetMaxPacketSize(size_t size) override {
        max_packet_size_ = size;
    }
    
    void SetPayloadType(uint8_t pt) override {
        payload_type_ = pt;
    }
    
    std::vector<std::shared_ptr<RtpPacket>> PackNalu(
        const uint8_t* nalu_data,
        size_t nalu_size,
        uint32_t timestamp,
        bool marker
    ) override {
        std::vector<std::shared_ptr<RtpPacket>> packets;
        
        // 计算MTU预留 (IP/UDP/RTP头)
        // IPv4: 20 + 8 + 12 = 40 bytes
        // IPv6: 40 + 8 + 12 = 60 bytes
        const size_t kRtpHeaderSize = 12;
        const size_t kUdpHeaderSize = 8;
        const size_t kIpHeaderSize = 20;
        const size_t kOverhead = kRtpHeaderSize + kUdpHeaderSize + kIpHeaderSize;
        
        size_t max_payload_size = max_packet_size_ > kOverhead 
            ? max_packet_size_ - kOverhead 
            : 1400;  // 默认
        
        if (nalu_size <= max_payload_size) {
            // 单一NALU包
            packets.push_back(PackSingleNalu(nalu_data, nalu_size, timestamp, marker));
        } else {
            // FU-A分片
            packets = PackFuASequence(nalu_data, nalu_size, timestamp, marker);
        }
        
        return packets;
    }
    
    std::shared_ptr<RtpPacket> PackFuA(
        const uint8_t* nalu_data,
        size_t nalu_size,
        size_t offset,
        uint32_t timestamp,
        bool marker
    ) override {
        // 1. 解析原始NALU头部
        uint8_t nalu_header = nalu_data[0];
        uint8_t nal_type = nalu_header & 0x1F;
        uint8_t nal_ref_idc = (nalu_header >> 5) & 0x03;
        
        // 2. 计算分片大小
        size_t max_payload_size = 1400 - 12;  // FU-A payload最大
        size_t fragment_size = std::min(nalu_size - offset, max_payload_size);
        
        // 3. 构建FU-A
        auto packet = std::make_shared<RtpPacket>();
        
        // RTP头部
        packet->header.marker = (offset + fragment_size >= nalu_size) ? marker : 0;
        packet->header.timestamp = timestamp;
        packet->header.payload_type = payload_type_;
        packet->header.sequence_number = next_seq_++;
        
        // FU指示符 (Fragmentation Unit Indicator)
        uint8_t fu_indicator = (nal_ref_idc << 5) | 0x1C | 28;  // FU-A type = 28
        packet->payload.push_back(fu_indicator);
        
        // FU头部 (Fragmentation Unit Header)
        uint8_t fu_header = 0;
        if (offset == 0) {
            fu_header |= 0x80;  // S (start) bit
        }
        if (offset + fragment_size >= nalu_size) {
            fu_header |= 0x40;  // E (end) bit
        }
        fu_header |= nal_type;  // 原始NALU类型
        packet->payload.push_back(fu_header);
        
        // FU载荷
        packet->payload.insert(
            packet->payload.end(),
            nalu_data + offset,
            nalu_data + offset + fragment_size
        );
        
        stats_.fu_a_packets++;
        
        return packet;
    }

private:
    size_t max_packet_size_ = 1400;
    uint8_t payload_type_ = 96;
    uint16_t next_seq_ = 0;
    
    PackerStats stats_;
};
```

### 5.4 视频组帧 (Video Assembler)

```cpp
// ============================================================================
// IVideoAssembler - 视频组帧接口
// ============================================================================
class IVideoAssembler : public std::enable_shared_from_this<IVideoAssembler> {
public:
    using Ptr = std::shared_ptr<IVideoAssembler>;
    
    virtual ~IVideoAssembler() = default;
    
    // 添加RTP包
    virtual void AddPacket(std::shared_ptr<RtpPacket> packet) = 0;
    
    // 获取完整帧
    virtual std::shared_ptr<EncodedFrame> GetFrame(int timeout_ms) = 0;
    
    // 尝试获取帧 (非阻塞)
    virtual std::shared_ptr<EncodedFrame> TryGetFrame() = 0;
    
    // 获取丢包数
    virtual uint32_t GetPacketsLost() const = 0;
    
    // 重置
    virtual void Reset() = 0;
};

// ============================================================================
// VideoAssembler 实现
// ============================================================================
class VideoAssembler : public IVideoAssembler {
public:
    VideoAssembler();
    ~VideoAssembler() override;
    
    void AddPacket(std::shared_ptr<RtpPacket> packet) override;
    std::shared_ptr<EncodedFrame> GetFrame(int timeout_ms) override;
    std::shared_ptr<EncodedFrame> TryGetFrame() override;
    uint32_t GetPacketsLost() const override;
    void Reset() override;

private:
    // 组帧逻辑:
    // 1. 根据sequence number排序
    // 2. 检测FU-A的start和end
    // 3. 拼接完整的NALU
    // 4. marker=1时交付完整帧
    
    struct Fragment {
        uint16_t seq;
        uint8_t fu_header;  // S+E bits
        std::vector<uint8_t> data;
    };
    
    std::map<uint16_t, Fragment> fragments_;  // 按seq排序
    uint16_t expected_seq_ = 0;
    uint32_t packets_lost_ = 0;
    bool in_frame_ = false;
    
    mutable std::mutex mutex_;
};
```

---

## 6. 端到端测试框架

### 6.1 设计目标

- **真实网络**: 两个端通过真实Socket通信
- **完整流程**: 端到端音视频通话
- **可重复**: 可配置的测试参数
- **可观测**: 详细的统计和日志

### 6.2 核心接口

```cpp
// ============================================================================
// EndToEndConfig - 测试配置
// ============================================================================
struct EndToEndConfig {
    // 本端配置
    SocketAddress local_rtp_addr{"0.0.0.0", 5000};
    SocketAddress local_rtcp_addr{"0.0.0.0", 5001};
    
    // 远端配置
    SocketAddress remote_rtp_addr{"127.0.0.1", 6000};
    SocketAddress remote_rtcp_addr{"127.0.0.1", 6001};
    
    // 媒体配置
    bool enable_audio = true;
    bool enable_video = true;
    
    // 音频参数
    uint8_t audio_payload_type = 111;  // Opus
    uint32_t audio_sample_rate = 48000;
    uint8_t audio_channels = 1;
    
    // 视频参数
    uint8_t video_payload_type = 96;  // H264
    uint32_t video_width = 640;
    uint32_t video_height = 480;
    uint32_t video_fps = 30;
    uint32_t video_bitrate_kbps = 1000;
    
    // 测试参数
    uint32_t test_duration_ms = 10000;
    bool loopback_mode = false;  // 环回模式 (本端发本端收)
};

// ============================================================================
// TestResult - 测试结果
// ============================================================================
struct TestResult {
    bool success = false;
    
    // 执行信息
    uint32_t duration_ms = 0;
    std::string error_message;
    
    // 音频统计
    AudioStats audio;
    
    // 视频统计
    VideoStats;
    
    // 网络统计
    NetworkStats network;
};

struct AudioStats {
    uint64_t sent = 0;
    uint64_t received = 0;
    uint64_t lost = 0;
    float loss_rate = 0.0f;
    uint32_t avg_latency_ms = 0;
};

struct VideoStats {
    uint64_t sent = 0;
    uint64_t received = 0;
    uint64_t lost = 0;
    float loss_rate = 0.0f;
    uint32_t avg_latency_ms = 0;
    uint32_t frames_decoded = 0;
};

struct NetworkStats {
    uint64_t rtp_packets_sent = 0;
    uint64_t rtp_packets_received = 0;
    uint64_t rtcp_packets_sent = 0;
    uint64_t rtcp_packets_received = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
};

// ============================================================================
// IEndToEndTest - 测试接口
// ============================================================================
class IEndToEndTest : public std::enable_shared_from_this<IEndToEndTest> {
public:
    using Ptr = std::shared_ptr<IEndToEndTest>;
    
    virtual ~IEndToEndTest() = default;
    
    // ---------- 配置 ----------
    virtual void SetConfig(const EndToEndConfig& config) = 0;
    virtual EndToEndConfig GetConfig() const = 0;
    
    // ---------- 生命周期 ----------
    virtual bool Initialize() = 0;
    virtual bool Run() = 0;
    virtual void Stop() = 0;
    virtual void Wait() = 0;
    
    // ---------- 状态 ----------
    virtual bool IsRunning() const = 0;
    virtual float GetProgress() const = 0;
    
    // ---------- 结果 ----------
    virtual TestResult GetResult() const = 0;
    
    // ---------- 回调 ----------
    virtual void SetOnProgress(std::function<void(float)> callback) = 0;
    virtual void SetOnComplete(std::function<void(const TestResult&)> callback) = 0;
    virtual void SetOnError(std::function<void(const std::string&)> callback) = 0;
};
```

### 6.3 测试框架实现

```cpp
// ============================================================================
// EndToEndTest 实现
// ============================================================================
class EndToEndTest : public IEndToEndTest {
public:
    EndToEndTest();
    ~EndToEndTest() override;
    
    void SetConfig(const EndToEndConfig& config) override {
        config_ = config;
    }
    
    EndToEndConfig GetConfig() const override {
        return config_;
    }
    
    bool Initialize() override;
    bool Run() override;
    void Stop() override;
    void Wait() override;
    
    bool IsRunning() const override {
        return running_;
    }
    
    float GetProgress() const override {
        if (config_.test_duration_ms == 0) return 0.0f;
        return static_cast<float>(elapsed_ms_.load()) / config_.test_duration_ms;
    }
    
    TestResult GetResult() const override {
        return result_;
    }
    
    // 回调设置
    void SetOnProgress(std::function<void(float)> callback) override {
        on_progress_ = callback;
    }
    
    void SetOnComplete(std::function<void(const TestResult&)> callback) override {
        on_complete_ = callback;
    }
    
    void SetOnError(std::function<void(const std::string&)> callback) override {
        on_error_ = callback;
    }

private:
    // 内部结构
    struct Endpoint {
        std::unique_ptr<RTPTransport> rtp_transport;
        std::unique_ptr<RtcpSession> rtcp_session;
        IStream::Ptr stream;
        std::unique_ptr<MediaSource> media_source;  // 生成测试数据
    };
    
    // 测试流程:
    // 1. 创建两个Endpoint (local, remote)
    // 2. 启动两个Stream
    // 3. 本地发送媒体数据
    // 4. 远端接收并统计
    // 5. 汇总结果
    
    EndToEndConfig config_;
    
    // 双端
    std::unique_ptr<Endpoint> local_endpoint_;
    std::unique_ptr<Endpoint> remote_endpoint_;
    
    // 控制
    std::atomic<bool> running_{false};
    std::atomic<uint32_t> elapsed_ms_{0};
    std::thread test_thread_;
    
    // 结果
    TestResult result_;
    
    // 回调
    std::function<void(float)> on_progress_;
    std::function<void(const TestResult&)> on_complete_;
    std::function<void(const std::string&)> on_error_;
};

// 关键实现要点:
// 1. 使用真实UDP Socket
// 2. 本端生成测试数据 (音频: 静音帧, 视频: 彩色帧)
// 3. 远端统计丢包、延迟
// 4. 支持loopback模式测试单端
```

### 6.4 测试用例

```cpp
// ============================================================================
// 测试用例示例
// ============================================================================

class E2ETestCases {
public:
    // 测试1: 本地环回
    static TestResult TestLoopback() {
        EndToEndConfig config;
        config.local_rtp_addr = {"127.0.0.1", 5000};
        config.remote_rtp_addr = {"127.0.0.1", 6000};
        config.loopback_mode = true;
        config.enable_video = true;
        config.test_duration_ms = 5000;
        
        auto test = std::make_shared<EndToEndTest>();
        test->SetConfig(config);
        test->Initialize();
        
        if (!test->Run()) {
            return test->GetResult();
        }
        
        test->Wait();
        return test->GetResult();
    }
    
    // 测试2: 双端通信
    static TestResult TestBidirectional() {
        EndToEndConfig config;
        config.local_rtp_addr = {"0.0.0.0", 5000};
        config.remote_rtp_addr = {"127.0.0.1", 6000};
        config.enable_audio = true;
        config.enable_video = true;
        config.test_duration_ms = 10000;
        
        auto test = std::make_shared<EndToEndTest>();
        test->SetConfig(config);
        test->Initialize();
        test->Run();
        test->Wait();
        
        return test->GetResult();
    }
    
    // 测试3: 仅音频
    static TestResult TestAudioOnly() {
        EndToEndConfig config;
        config.enable_video = false;
        config.enable_audio = true;
        config.test_duration_ms = 5000;
        
        auto test = std::make_shared<EndToEndTest>();
        test->SetConfig(config);
        test->Initialize();
        test->Run();
        test->Wait();
        
        return test->GetResult();
    }
};

---

## 7. 目录结构

```
include/minirtc/
├── base/
│   ├── common.h
│   ├── ref_count.h
│   └── ring_buffer.h
├── stream/
│   ├── stream_manager.h
│   ├── stream.h
│   ├── track.h
│   └── stream_types.h
├── transport/
│   ├── rtp_transport.h
│   ├── rtcp_session.h
│   └── transport_config.h
├── jitter_buffer/
│   ├── jitter_buffer.h
│   ├── pass_through_jitter.h
│   └── jitter_buffer_config.h
├── packer/
│   ├── rtp_packer.h
│   ├── h264_packer.h
│   ├── opus_packer.h
│   └── packer_types.h
├── assembler/
│   ├── video_assembler.h
│   └── frame_types.h
├── codec/
│   ├── codec.h
│   ├── h264_encoder.h
│   ├── h264_decoder.h
│   ├── opus_encoder.h
│   └── opus_decoder.h
├── media/
│   ├── media_source.h
│   ├── media_sink.h
│   └── media_types.h
└── test/
    ├── e2e_test.h
    └── e2e_test_types.h

src/
├── stream/
│   ├── stream_manager.cc
│   ├── stream.cc
│   └── track.cc
├── transport/
│   ├── rtp_transport.cc
│   └── rtcp_session.cc
├── jitter_buffer/
│   ├── jitter_buffer.cc
│   └── pass_through_jitter.cc
├── packer/
│   ├── rtp_packer.cc
│   ├── h264_packer.cc
│   └── opus_packer.cc
├── assembler/
│   └── video_assembler.cc
└── test/
    └── e2e_test.cc
```

---

## 8. 实现计划

### Phase 1: Stream/Track抽象 (优先级: 高)
- [ ] StreamManager单例实现
- [ ] Stream类基本功能
- [ ] Track类基本功能
- [ ] CallBuilder简化API

### Phase 2: Jitter Buffer透传模式 (优先级: 高)
- [ ] PassThroughJitterBuffer实现
- [ ] Lock-free queue集成
- [ ] 统计功能

### Phase 3: H.264 RTP打包 (优先级: 中)
- [ ] H264Packer单NALU打包
- [ ] FU-A分片打包
- [ ] STAP-A聚合打包
- [ ] VideoAssembler组帧

### Phase 4: 端到端测试框架 (优先级: 中)
- [ ] RTPTransport基础
- [ ] EndToEndTest实现
- [ ] 测试用例
- [ ] 性能测试

---

## 9. 关键设计决策

### 9.1 透传模式 vs 自适应模式

| 特性 | 透传模式 | 自适应模式 |
|------|----------|------------|
| 延迟 | 0ms | 20-200ms |
| 复杂度 | 简单 | 复杂 |
| 抗抖动 | 无 | 强 |
| 适用场景 | 实时交互 | 直播/录播 |

**决策**: 首期实现透传模式，降低复杂度，确保实时性。

### 9.2 Lock-Free Queue选择

考虑:
- moodycamel::ConcurrentQueue (推荐)
- boost::lockfree::queue
- 自实现SPSC

**决策**: 使用moodycamel::ConcurrentQueue，成熟稳定，性能优秀。

### 9.3 内存管理

- 使用std::shared_ptr管理RTP包生命周期
- 对象池预分配减少分配开销
- 零拷贝设计 (避免不必要的数据复制)

---

## 10. 总结

本架构设计 (架构师B版本) 特点:

1. **Stream/Track抽象**: 简化通话创建，支持一行代码通话
2. **Jitter Buffer透传**: 零延迟透传，实时交互首选
3. **完整H.264支持**: 单NALU、FU-A分片、STAP聚合、组帧
4. **端到端测试框架**: 真实Socket通信，可靠的测试覆盖
5. **性能优化**: lock-free queue，对象池，零拷贝

与架构师A版本的区别:
- 更详细的实现细节
- 强调性能优化
- 更完整的测试框架设计
- 更多的代码示例

