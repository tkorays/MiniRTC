# MiniRTC 新阶段架构设计文档 v1.0 (架构师B视角)

**版本**: 1.0
**日期**: 2026-03-11
**状态**: 初稿设计
**视角**: 互补设计 - 注重简洁性、流程化、易用性

---

## 1. 设计理念 (与架构师A的互补)

架构师A的设计注重：
- 详细的接口抽象和工厂模式
- 完善的状态机和回调机制
- 高度可配置性和可扩展性

架构师B的补充视角：
- **流程驱动**: 从用户使用场景出发，简化为"创建通话 → 启动 → 停止"三步
- **最小化接口**: 提供高层抽象，降低集成复杂度
- **内聚优先**: 将相关功能聚合成独立模块，减少跨模块依赖
- **透传优先**: Jitter Buffer和视频打包先做透传，后续再增强

---

## 2. 模块设计总览

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         MiniRTC 新阶段架构                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                         Call (高层入口)                              │   │
│  │              简化API: CreateCall → Start → Stop                   │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│           ┌────────────────────────┼────────────────────────┐              │
│           ▼                        ▼                        ▼              │
│  ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐      │
│  │   LocalStream  │     │  RemoteStream   │     │  MediaRouter    │      │
│  │  (本地媒体流)   │     │  (远端媒体流)    │     │   (媒体路由)     │      │
│  └────────┬────────┘     └────────┬────────┘     └────────┬────────┘      │
│           │                       │                       │                │
│  ┌────────┴────────┐     ┌────────┴────────┐     ┌────────┴────────┐      │
│  │ VideoTrack      │     │ VideoTrack      │     │ JitterBuffer   │      │
│  │ AudioTrack      │     │ AudioTrack      │     │ (音视频)        │      │
│  └─────────────────┘     └─────────────────┘     └─────────────────┘      │
│                                                                       ▲    │
│  ┌───────────────────────────────────────────────────────────────────┘    │
│  │                      Transport (已有模块)                            │
│  │   RTPTransport → RtpPacket → NetworkInterface → Socket             │
│  └───────────────────────────────────────────────────────────────────┘    │
│                                                                       │    │
│  ┌───────────────────────────────────────────────────────────────────┐    │
│  │                    VideoPacker (新增模块)                         │
│  │     H.264 NALU → RTP Packets (Single NALU / FU-A)               │
│  └───────────────────────────────────────────────────────────────────┘    │
│                                                                       │    │
│  ┌───────────────────────────────────────────────────────────────────┐    │
│  │                    E2E Test Framework (新增)                      │
│  │        PeerA <--Socket--> PeerB (RTP/RTCP)                      │
│  └───────────────────────────────────────────────────────────────────┘    │
│                                                                       │    │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Stream/Track 抽象设计

### 3.1 设计目标

- **简化通话创建**: 用户只需几行代码即可创建1对1通话
- **统一媒体管理**: Track管理单一轨道，Stream管理会话
- **解耦传输**: Track与传输层通过抽象接口解耦

### 3.2 核心接口

```cpp
namespace minirtc {

// ============================================================================
// Track - 单一流轨道
// ============================================================================

/// 媒体类型
enum class MediaType {
    kAudio,
    kVideo
};

/// Track 状态
enum class TrackState {
    kIdle,
    kReady,
    kActive,
    kMuted,
    kStopped
};

/// Track 接口
class ITrack {
public:
    virtual ~ITrack() = default;
    
    // 基础信息
    virtual MediaType GetMediaType() const = 0;
    virtual uint32_t GetSSRC() const = 0;
    virtual TrackState GetState() const = 0;
    
    // RTP操作
    virtual void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) = 0;
    virtual std::shared_ptr<RtpPacket> GetRtpPacketToSend() = 0;
    
    // 媒体操作
    virtual void Mute() = 0;
    virtual void Unmute() = 0;
    virtual bool IsMuted() const = 0;
    
    // 配置
    virtual void SetPayloadType(uint8_t pt) = 0;
    virtual uint8_t GetPayloadType() const = 0;
    virtual void SetClockRate(uint32_t rate) = 0;
};

/// VideoTrack - 视频轨道
class IVideoTrack : public ITrack {
public:
    MediaType GetMediaType() const override { return MediaType::kVideo; }
    
    // 视频特有操作
    virtual void SetRtpPacker(std::shared_ptr<IRtpPacker> packer) = 0;
    virtual void SetJitterBuffer(std::shared_ptr<IJitterBuffer> buffer) = 0;
    virtual void OnEncodedFrame(std::shared_ptr<EncodedFrame> frame) = 0;
};

/// AudioTrack - 音频轨道  
class IAudioTrack : public ITrack {
public:
    MediaType GetMediaType() const override { return MediaType::kAudio; }
    
    // 音频特有操作
    virtual void SetJitterBuffer(std::shared_ptr<IJitterBuffer> buffer) = 0;
    virtual void OnEncodedFrame(std::shared_ptr<EncodedFrame> frame) = 0;
};

// ============================================================================
// Stream - 包含多个Track的会话流
// ============================================================================

/// Stream 类型
enum class StreamType {
    kLocal,      // 本地发送流
    kRemote      // 远端接收流
};

/// Stream 接口
class IStream {
public:
    virtual ~IStream() = default;
    
    // 基础信息
    virtual uint64_t GetId() const = 0;
    virtual StreamType GetType() const = 0;
    virtual std::string GetLabel() const = 0;
    
    // Track管理
    virtual std::shared_ptr<IVideoTrack> GetVideoTrack() const = 0;
    virtual std::shared_ptr<IAudioTrack> GetAudioTrack() const = 0;
    virtual void AddTrack(std::shared_ptr<ITrack> track) = 0;
    virtual void RemoveTrack(MediaType type) = 0;
    
    // 状态
    virtual bool HasVideo() const = 0;
    virtual bool HasAudio() const = 0;
};

/// LocalStream - 本地媒体流 (发送端)
class ILocalStream : public IStream {
public:
    StreamType GetType() const override { return StreamType::kLocal; }
    
    // 采集控制
    virtual void StartCapture() = 0;
    virtual void StopCapture() = 0;
    
    // 编码后发送
    virtual void SendEncodedFrame(std::shared_ptr<EncodedFrame> frame) = 0;
};

/// RemoteStream - 远端媒体流 (接收端)
class IRemoteStream : public IStream {
public:
    StreamType GetType() const override { return StreamType::kRemote; }
    
    // 接收处理
    virtual void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) = 0;
    
    // 发送给渲染
    virtual void OnDecodedFrame(std::shared_ptr<VideoFrame> frame) = 0;
    virtual void OnDecodedFrame(std::shared_ptr<AudioFrame> frame) = 0;
};

}  // namespace minirtc
```

### 3.3 关键实现类

```cpp
namespace minirtc {

/// VideoTrack 实现
class VideoTrack : public IVideoTrack, 
                   public std::enable_shared_from_this<VideoTrack> {
public:
    explicit VideoTrack(uint32_t ssrc);
    
    // ITrack 接口
    MediaType GetMediaType() const override { return MediaType::kVideo; }
    uint32_t GetSSRC() const override { return ssrc_; }
    TrackState GetState() const override { return state_.load(); }
    
    void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) override;
    std::shared_ptr<RtpPacket> GetRtpPacketToSend() override;
    
    void Mute() override;
    void Unmute() override;
    bool IsMuted() const override;
    
    void SetPayloadType(uint8_t pt) override { payload_type_ = pt; }
    uint8_t GetPayloadType() const override { return payload_type_; }
    void SetClockRate(uint32_t rate) override { clock_rate_ = rate; }
    
    // IVideoTrack 接口
    void SetRtpPacker(std::shared_ptr<IRtpPacker> packer) override;
    void SetJitterBuffer(std::shared_ptr<IJitterBuffer> buffer) override;
    void OnEncodedFrame(std::shared_ptr<EncodedFrame> frame) override;

private:
    uint32_t ssrc_;
    std::atomic<TrackState> state_;
    std::atomic<bool> muted_{false};
    
    uint8_t payload_type_ = 96;
    uint32_t clock_rate_ = 90000;
    
    std::shared_ptr<IRtpPacker> packer_;
    std::shared_ptr<IJitterBuffer> jitter_buffer_;
    
    moodycamel::ConcurrentQueue<std::shared_ptr<RtpPacket>> send_queue_;
    
    std::atomic<uint64_t> packets_sent_{0};
    std::atomic<uint64_t> bytes_sent_{0};
};

/// LocalStream 实现
class LocalStream : public ILocalStream {
public:
    LocalStream();
    explicit LocalStream(const std::string& label);
    
    uint64_t GetId() const override { return id_; }
    StreamType GetType() const override { return StreamType::kLocal; }
    std::string GetLabel() const override { return label_; }
    
    std::shared_ptr<IVideoTrack> GetVideoTrack() const override;
    std::shared_ptr<IAudioTrack> GetAudioTrack() const override;
    
    void AddTrack(std::shared_ptr<ITrack> track) override;
    void RemoveTrack(MediaType type) override;
    
    bool HasVideo() const override;
    bool HasAudio() const override;
    
    void StartCapture() override;
    void StopCapture() override;
    void SendEncodedFrame(std::shared_ptr<EncodedFrame> frame) override;

private:
    uint64_t id_;
    std::string label_;
    
    std::shared_ptr<VideoTrack> video_track_;
    std::shared_ptr<AudioTrack> audio_track_;
    
    std::shared_ptr<IEncoder> video_encoder_;
    std::shared_ptr<IEncoder> audio_encoder_;
    
    std::shared_ptr<IVideoCapture> video_capture_;
    std::shared_ptr<IAudioCapture> audio_capture_;
};

/// RemoteStream 实现
class RemoteStream : public IRemoteStream {
public:
    RemoteStream();
    explicit RemoteStream(uint64_t id, const std::string& label);
    
    uint64_t GetId() const override { return id_; }
    StreamType GetType() const override { return StreamType::kRemote; }
    std::string GetLabel() const override { return label_; }
    
    std::shared_ptr<IVideoTrack> GetVideoTrack() const override;
    std::shared_ptr<IAudioTrack> GetAudioTrack() const override;
    
    void AddTrack(std::shared_ptr<ITrack> track) override;
    void RemoveTrack(MediaType type) override;
    
    bool HasVideo() const override;
    bool HasAudio() const override;
    
    void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) override;
    void OnDecodedFrame(std::shared_ptr<VideoFrame> frame) override;
    void OnDecodedFrame(std::shared_ptr<AudioFrame> frame) override;

private:
    uint64_t id_;
    std::string label_;
    
    std::shared_ptr<VideoTrack> video_track_;
    std::shared_ptr<AudioTrack> audio_track_;
    
    std::shared_ptr<IDecoder> video_decoder_;
    std::shared_ptr<IDecoder> audio_decoder_;
    
    std::shared_ptr<IVideoRenderer> video_renderer_;
    std::shared_ptr<IAudioPlayer> audio_player_;
};

}  // namespace minirtc
```

---

## 4. 高层 Call API 设计

### 4.1 简化接口

```cpp
namespace minirtc {

/// Call 配置
struct CallConfig {
    bool enable_video = true;
    bool enable_audio = true;
    
    VideoEncoderConfig video_encoder;
    AudioEncoderConfig audio_encoder;
    TransportConfig transport;
    
    std::function<void(std::shared_ptr<RemoteStream>)> on_remote_stream;
    std::function<void(const std::string&)> on_error;
};

enum class CallState {
    kIdle,
    kConnecting,
    kConnected,
    kDisconnected,
    kFailed
};

/// Call 接口 (高层入口)
class ICall {
public:
    virtual ~ICall() = default;
    
    virtual void CreateLocalStream() = 0;
    virtual std::shared_ptr<LocalStream> GetLocalStream() const = 0;
    
    virtual void Start() = 0;
    virtual void Stop() = 0;
    
    virtual std::vector<std::shared_ptr<RemoteStream>> GetRemoteStreams() const = 0;
    
    virtual CallState GetState() const = 0;
    
    virtual void SetRemoteAddress(const NetworkAddress& addr) = 0;
};

/// 创建1对1通话
std::shared_ptr<ICall> CreateCall(const CallConfig& config);

}  // namespace minirtc
```

### 4.2 使用示例

```cpp
void MakeCall() {
    CallConfig config;
    config.enable_video = true;
    config.enable_audio = true;
    
    config.video_encoder.codec_type = CodecType::kH264;
    config.audio_encoder.codec_type = CodecType::kOpus;
    
    NetworkAddress remote("192.168.1.100", 5000);
    
    auto call = CreateCall(config);
    call->SetRemoteAddress(remote);
    call->Start();
    
    // ... 使用 ...
    
    call->Stop();
}
```

---

## 5. Jitter Buffer 设计

### 5.1 接口设计

```cpp
namespace minirtc {

enum class JitterBufferMode {
    kPassThrough,    // 透传模式
    kBuffering,       // 缓冲模式
    kAdaptive         // 自适应模式
};

struct JitterBufferConfig {
    JitterBufferMode mode = JitterBufferMode::kPassThrough;
    size_t min_buffer_ms = 20;
    size_t max_buffer_ms = 200;
    bool enable_stats = true;
};

struct JitterBufferStats {
    uint64_t packets_received = 0;
    uint64_t packets_delivered = 0;
    uint64_t packets_discarded = 0;
    uint64_t buffer_overflows = 0;
    uint64_t buffer_underflows = 0;
    int current_buffer_ms = 0;
};

class IJitterBuffer {
public:
    virtual ~IJitterBuffer() = default;
    
    virtual void SetConfig(const JitterBufferConfig& config) = 0;
    virtual JitterBufferConfig GetConfig() const = 0;
    
    virtual void PutPacket(std::shared_ptr<RtpPacket> packet) = 0;
    virtual std::shared_ptr<RtpPacket> GetPacket(int timeout_ms = 0) = 0;
    
    virtual JitterBufferStats GetStats() const = 0;
    virtual void ResetStats() = 0;
    
    virtual void SetSyncGroup(uint32_t ssrc) = 0;
    virtual void SyncTo(uint32_t audio_ssrc, uint64_t timestamp_us) = 0;
};

}  // namespace minirtc
```

### 5.2 透传模式实现

```cpp
class PassThroughJitterBuffer : public IJitterBuffer {
public:
    explicit PassThroughJitterBuffer(MediaType media_type)
        : media_type_(media_type) {}
    
    void SetConfig(const JitterBufferConfig& config) override {
        config_ = config;
    }
    
    void PutPacket(std::shared_ptr<RtpPacket> packet) override {
        packets_received_++;
        // 透传: 直接放入队列
        queue_.enqueue(packet);
    }
    
    std::shared_ptr<RtpPacket> GetPacket(int timeout_ms = 0) override {
        std::shared_ptr<RtpPacket> packet;
        if (queue_.try_dequeue(packet)) {
            packets_delivered_++;
            return packet;
        }
        return nullptr;
    }
    
    JitterBufferStats GetStats() const override {
        return {
            .packets_received = packets_received_.load(),
            .packets_delivered = packets_delivered_.load(),
            .packets_discarded = packets_discarded_.load(),
        };
    }
    
    void ResetStats() override {
        packets_received_ = 0;
        packets_delivered_ = 0;
        packets_discarded_ = 0;
    }
    
    void SetSyncGroup(uint32_t ssrc) override {
        sync_ssrc_ = ssrc;
    }
    
    void SyncTo(uint32_t audio_ssrc, uint64_t timestamp_us) override {
        sync_timestamp_us_ = timestamp_us;
    }

private:
    MediaType media_type_;
    JitterBufferConfig config_;
    
    moodycamel::ConcurrentQueue<std::shared_ptr<RtpPacket>> queue_;
    
    std::atomic<uint64_t> packets_received_{0};
    std::atomic<uint64_t> packets_delivered_{0};
    std::atomic<uint64_t> packets_discarded_{0};
    
    std::atomic<uint32_t> sync_ssrc_{0};
    std::atomic<uint64_t> sync_timestamp_us_{0};
};
```

---

## 6. 视频打包/组帧设计 (H.264)

### 6.1 RTP打包器接口

```cpp
namespace minirtc {

class IRtpPacker {
public:
    virtual ~IRtpPacker() = default;
    
    virtual void SetMaxPacketSize(size_t size) = 0;
    virtual void SetPayloadType(uint8_t pt) = 0;
    virtual void SetSSRC(uint32_t ssrc) = 0;
    virtual void SetTimestamp(uint32_t timestamp, uint32_t clock_rate) = 0;
    
    virtual std::vector<std::shared_ptr<RtpPacket>> Pack(
        const uint8_t* data, size_t size, bool marker) = 0;
    
    virtual std::vector<std::shared_ptr<RtpPacket>> Pack(
        std::shared_ptr<EncodedFrame> frame) = 0;
};

enum class H264NaluType {
    kNonIDR = 1,
    kIDR = 5,
    kSEI = 6,
    kSPS = 7,
    kPPS = 8,
    kFU_A = 28,
    kFU_B = 29,
};

}  // namespace minirtc
```

### 6.2 H.264 RTP打包器实现

```cpp
namespace minirtc {

class H264RtpPacker : public IRtpPacker {
public:
    H264RtpPacker() : seq_(0) {}
    
    void SetMaxPacketSize(size_t size) override {
        max_packet_size_ = size;
    }
    
    void SetPayloadType(uint8_t pt) override {
        payload_type_ = pt;
    }
    
    void SetSSRC(uint32_t ssrc) override {
        ssrc_ = ssrc;
    }
    
    void SetTimestamp(uint32_t timestamp, uint32_t clock_rate) override {
        timestamp_ = timestamp;
        clock_rate_ = clock_rate;
    }
    
    std::vector<std::shared_ptr<RtpPacket>> Pack(
        const uint8_t* data, size_t size, bool marker) override {
        
        if (size <= max_packet_size_ - 12) {
            // 单个NALU包
            return { PackSingleNalu(data, size, marker) };
        } else {
            // FU-A分片
            return PackFuA(data, size, marker);
        }
    }
    
    std::vector<std::shared_ptr<RtpPacket>> Pack(
        std::shared_ptr<EncodedFrame> frame) override {
        return Pack(frame->data.data(), frame->data.size(), frame->is_keyframe);
    }

private:
    std::shared_ptr<RtpPacket> PackSingleNalu(
        const uint8_t* nalu_data, size_t nalu_size, bool marker) {
        
        auto packet = std::make_shared<RtpPacket>();
        packet->SetSSRC(ssrc_);
        packet->SetSequenceNumber(seq_++);
        packet->SetTimestamp(timestamp_);
        packet->SetMarker(marker);
        packet->SetPayloadType(payload_type_);
        
        // NALU直接作为载荷
        packet->SetPayload(nalu_data, nalu_size);
        
        return packet;
    }
    
    std::vector<std::shared_ptr<RtpPacket>> PackFuA(
        const uint8_t* nalu_data, size_t nalu_size, bool marker) {
        
        std::vector<std::shared_ptr<RtpPacket>> packets;
        
        // 获取NALU类型
        uint8_t nal_type = nalu_data[0] & 0x1F;
        
        size_t payload_size = max_packet_size_ - 12 - 2; // RTP头 + FU指示 + FU头
        size_t offset = 0;
        bool start = true;
        
        while (offset < nalu_size) {
            size_t remaining = nalu_size - offset;
            bool end = (remaining <= payload_size);
            
            auto packet = std::make_shared<RtpPacket>();
            packet->SetSSRC(ssrc_);
            packet->SetSequenceNumber(seq_++);
            packet->SetTimestamp(timestamp_);
            packet->SetMarker(end && marker);
            packet->SetPayloadType(payload_type_);
            
            // FU指示字节
            uint8_t fui_indicator = 28 << 3;  // FU-A类型
            
            // FU头字节
            uint8_t fu_header = 0;
            if (start) fu_header |= 0x80;  // S位
            if (end) fu_header |= 0x40;    // E位
            fu_header |= nal_type;          // NALU类型
            
            // 构建载荷: FU指示 + FU头 + 片段
            std::vector<uint8_t> payload;
            payload.push_back(fui_indicator);
            payload.push_back(fu_header);
            
            size_t copy_size = std::min(payload_size, remaining);
            payload.insert(payload.end(), nalu_data + offset, nalu_data + offset + copy_size);
            
            packet->SetPayload(payload.data(), payload.size());
            packets.push_back(packet);
            
            offset += copy_size;
            start = false;
        }
        
        return packets;
    }

private:
    uint32_t ssrc_ = 0;
    uint16_t seq_ = 0;
    uint32_t timestamp_ = 0;
    uint32_t clock_rate_ = 90000;
    uint8_t payload_type_ = 96;
    size_t max_packet_size_ = 1400;
};

}  // namespace minirtc
```

### 6.3 H.264 RTP组帧器

```cpp
namespace minirtc {

class IRtpDefragmenter {
public:
    virtual ~IRtpDefragmenter() = default;
    
    virtual void AddPacket(std::shared_ptr<RtpPacket> packet) = 0;
    virtual std::vector<std::shared_ptr<EncodedFrame>> GetFrames() = 0;
    virtual void SetTimeout(uint32_t timeout_ms) = 0;
    virtual void CheckTimeout() = 0;
};

class H264RtpDefragmenter : public IRtpDefragmenter {
public:
    H264RtpDefragmenter() : timeout_ms_(100) {}
    
    void AddPacket(std::shared_ptr<RtpPacket> packet) override {
        const uint8_t* payload = packet->GetPayload();
        size_t payload_size = packet->GetPayloadSize();
        
        if (payload_size < 2) return;
        
        uint8_t fui_indicator = payload[0];
        uint8_t fu_header = payload[1];
        
        uint8_t nal_type = fui_indicator & 0x1F;
        
        if (nal_type == 28) {  // FU-A
            bool start = (fu_header & 0x80) != 0;
            bool end = (fu_header & 0x40) != 0;
            uint8_t original_nal_type = fu_header & 0x1F;
            
            if (start) {
                // 开始新的分片
                FragmentState frag;
                frag.nal_type = original_nal_type;
                frag.first_seq = packet->GetSequenceNumber();
                frag.first_arrival_us = GetCurrentTimestampUs();
                frag.buffer.push_back(original_nal_type);  // 恢复NALU头
                frag.buffer.insert(frag.buffer.end(), 
                    payload + 2, payload + payload_size);
                fragments_[packet->GetSSRC()] = frag;
            } else {
                // 继续分片
                auto it = fragments_.find(packet->GetSSRC());
                if (it != fragments_.end()) {
                    it->second.buffer.insert(it->second.buffer.end(),
                        payload + 2, payload + payload_size);
                    
                    if (end) {
                        // 完成组帧
                        auto frame = std::make_shared<EncodedFrame>();
                        frame->data = it->second.buffer;
                        frame->is_keyframe = (original_nal_type == 5);
                        completed_frames_.push_back(frame);
                        fragments_.erase(it);
                    }
                }
            }
        } else {
            // 单个NALU包
            auto frame = std::make_shared<EncodedFrame>();
            frame->data.assign(payload, payload + payload_size);
            frame->is_keyframe = (nal_type == 5);
            completed_frames_.push_back(frame);
        }
    }
    
    std::vector<std::shared_ptr<EncodedFrame>> GetFrames() override {
        auto frames = completed_frames_;
        completed_frames_.clear();
        return frames;
    }
    
    void SetTimeout(uint32_t timeout_ms) override {
        timeout_ms_ = timeout_ms;
    }
    
    void CheckTimeout() override {
        uint64_t now = GetCurrentTimestampUs();
        
        for (auto it = fragments_.begin(); it != fragments_.end();) {
            if (now - it->second.first_arrival_us > timeout_ms_ * 1000) {
                it = fragments_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    struct FragmentState {
        uint8_t nal_type = 0;
        std::vector<uint8_t> buffer;
        uint16_t first_seq = 0;
        uint64_t first_arrival_us = 0;
    };
    
    std::map<uint32_t, FragmentState> fragments_;
    std::mutex mutex_;
    uint32_t timeout_ms_ = 100;
    std::vector<std::shared_ptr<EncodedFrame>> completed_frames_;
};

}  // namespace minirtc
```

---

## 7. 端到端测试框架设计

### 7.1 测试Peer

```cpp
namespace minirtc {
namespace testing {

class Peer {
public:
    Peer();
    ~Peer();
    
    void SetLocalPort(uint16_t rtp_port, uint16_t rtcp_port = 0);
    void SetRemoteAddress(const NetworkAddress& addr);
    
    void SetCall(std::shared_ptr<ICall> call);
    std::shared_ptr<ICall> GetCall() const;
    
    void Start();
    void Stop();
    
    uint64_t GetPacketsSent() const;
    uint64_t GetPacketsReceived() const;
    
    void SendRtpPacket(std::shared_ptr<RtpPacket> packet);
    
    using RtpPacketHandler = std::function<void(std::shared_ptr<RtpPacket>)>;
    void SetRtpHandler(RtpPacketHandler handler);
    
    void SendRtcpPacket(const uint8_t* data, size_t size);
    using RtcpPacketHandler = std::function<void(const uint8_t*, size_t)>;
    void SetRtcpHandler(RtcpPacketHandler handler);

private:
    void ReceiveLoop();

    std::shared_ptr<INetworkInterface> rtp_socket_;
    std::shared_ptr<INetworkInterface> rtcp_socket_;
    
    NetworkAddress local_rtp_addr_;
    NetworkAddress remote_addr_;
    
    std::shared_ptr<ICall> call_;
    
    std::thread receive_thread_;
    std::atomic<bool> running_{false};
    
    RtpPacketHandler rtp_handler_;
    RtcpPacketHandler rtcp_handler_;
    
    std::atomic<uint64_t> packets_sent_{0};
    std::atomic<uint64_t> packets_received_{0};
};

}  // namespace testing
}  // namespace minirtc
```

### 7.2 测试场景

```cpp
namespace minirtc {
namespace testing {

class CallScenario {
public:
    CallScenario();
    ~CallScenario();
    
    void SetPeerA(std::shared_ptr<Peer> peer);
    void SetPeerB(std::shared_ptr<Peer> peer);
    
    void SetVideoEnabled(bool enabled);
    void SetAudioEnabled(bool enabled);
    
    void Run(Duration timeout);
    void RunUntil(std::function<bool()> condition, Duration timeout);
    
    bool VerifyPacketsExchanged(size_t min_packets = 10);
    bool VerifyNoPacketLoss();
    bool VerifyMediaFlow();

private:
    std::shared_ptr<Peer> peer_a_;
    std::shared_ptr<Peer> peer_b_;
    
    bool video_enabled_ = true;
    bool audio_enabled_ = true;
};

/// 网络条件模拟
struct NetworkCondition {
    int latency_ms = 0;
    int jitter_ms = 0;
    float packet_loss_rate = 0.0f;
    int bandwidth_bps = 0;
    float corruption_rate = 0.0f;
};

class NetworkEmulator {
public:
    NetworkEmulator();
    
    void SetCondition(const NetworkCondition& condition);
    NetworkCondition GetCondition() const;
    
    void ApplyTo(Peer* peer);
    std::optional<NetworkPacket> Process(NetworkPacket packet);

private:
    NetworkCondition condition_;
    std::mt19937 rng_;
};

}  // namespace testing
}  // namespace minirtc
```

### 7.3 测试用例示例

```cpp
namespace minirtc {
namespace testing {

TEST(CallE2ETest, BasicVideoCall) {
    auto peer_a = std::make_shared<Peer>();
    auto peer_b = std::make_shared<Peer>();
    
    peer_a->SetLocalPort(5000, 5001);
    peer_b->SetLocalPort(6000, 6001);
    
    peer_a->SetRemoteAddress(NetworkAddress("127.0.0.1", 6000));
    peer_b->SetRemoteAddress(NetworkAddress("127.0.0.1", 5000));
    
    CallScenario scenario;
    scenario.SetPeerA(peer_a);
    scenario.SetPeerB(peer_b);
    
    scenario.Run(Duration::Seconds(10));
    
    EXPECT_TRUE(scenario.VerifyPacketsExchanged(100));
}

TEST(VideoPackerTest, FU_AFragmentation) {
    H264RtpPacker packer;
    packer.SetMaxPacketSize(1400);
    packer.SetPayloadType(96);
    packer.SetSSRC(12345);
    packer.SetTimestamp(0, 90000);
    
    std::vector<uint8_t> idr_frame(3000, 0x65);  // NALU type 5 = IDR
    
    auto packets = packer.Pack(idr_frame.data(), idr_frame.size(), true);
    
    ASSERT_EQ(packets.size(), 3);
    
    EXPECT_TRUE(packets[0]->GetPayload()[1] & 0x80);  // S位
    EXPECT_FALSE(packets[0]->GetPayload()[1] & 0x40); // E位
    EXPECT_TRUE(packets[2]->GetPayload()[1] & 0x40);  // E位
}

}  // namespace testing
}  // namespace minirtc
```

---

## 8. 线程模型

### 8.1 线程划分

| 线程 | 职责 | 数据流 |
|------|------|--------|
| 主线程 | Call API调用、状态管理 | 控制命令 |
| 采集线程 | 音视频采集 | Raw Frame → Encoder |
| 发送线程 | 编码 + RTP打包 + 发送 | Encoded Frame → RTP → Socket |
| 接收线程 | RTP接收 + 解包 + 解码 | Socket → RTP → Decoded Frame |
| RTCP线程 | 统计、NACK、FEC | 周期性RTCP包 |

### 8.2 数据流

```
采集线程              发送线程              接收线程              渲染线程
   │                    │                    │                    │
   ▼                    ▼                    ▼                    ▼
┌──────┐          ┌──────────┐         ┌──────────┐          ┌──────────┐
│Camera│─────────▶│ Encoder  │─────────▶│RTP Pack │─────────▶│  Socket  │
│ Mic  │          │          │         │         │          │   (UDP)  │
└──────┘          └──────────┘         └──────────┘          └──────┬───┘
                                                                    │
                                                                    ▼
┌──────┐          ┌──────────┐         ┌──────────┐          ┌──────────┐
│Screen│◀─────────│ Decoder  │◀────────│Depacket │◀─────────│  Socket  │
│Spk   │          │          │         │         │          │   (UDP)  │
└──────┘          └──────────┘         └──────────┘          └──────────┘
```

---

## 9. 关键数据结构汇总

### 9.1 新增类型

```cpp
// minirtc/types.h 新增
enum class MediaType { kAudio, kVideo };
enum class TrackState { kIdle, kReady, kActive, kMuted, kStopped };
enum class StreamType { kLocal, kRemote };
enum class CallState { kIdle, kConnecting, kConnected, kDisconnected, kFailed };
enum class JitterBufferMode { kPassThrough, kBuffering, kAdaptive };
enum class H264NaluType { kNonIDR = 1, kIDR = 5, kSEI = 6, kSPS = 7, kPPS = 8, kFU_A = 28, kFU_B = 29 };

struct JitterBufferConfig { ... };
struct JitterBufferStats { ... };
struct CallConfig { ... };
struct NetworkCondition { ... };
```

### 9.2 文件结构

```
src/
├── CMakeLists.txt
├── call/
│   ├── call.h
│   ├── call.cc
│   ├── local_stream.h
│   ├── local_stream.cc
│   ├── remote_stream.h
│   └── remote_stream.cc
├── track/
│   ├── track.h
│   ├── video_track.h
│   ├── video_track.cc
│   ├── audio_track.h
│   └── audio_track.cc
├── jitter_buffer/
│   ├── jitter_buffer.h
│   ├── pass_through_jitter_buffer.h
│   └── pass_through_jitter_buffer.cc
├── video_packer/
│   ├── rtp_packer.h
│   ├── h264_rtp_packer.h
│   ├── h264_rtp_packer.cc
│   ├── rtp_defragmenter.h
│   └── h264_rtp_defragmenter.cc
└── testing/
    ├── peer.h
    ├── peer.cc
    ├── call_scenario.h
    ├── call_scenario.cc
    ├── network_emulator.h
    └── network_emulator.cc
```

---

## 10. 与架构师A设计的对比与互补

| 维度 | 架构师A | 架构师B |
|------|---------|---------|
| **设计重点** | 模块解耦、工厂模式、状态机 | 简化API、流程驱动、透传优先 |
| **Track/Stream** | 已有设计(接口层) | 提供简化实现 + 高层Call |
| **JitterBuffer** | 未涉及 | 透传模式优先 |
| **视频打包** | 未涉及 | H.264 FU-A实现 |
| **测试框架** | 单元测试 + Mock | 真实Socket E2E测试 |
| **API风格** | 底层接口 + 配置 | 高层入口 (CreateCall) |

**互补策略**: 
- 架构师A提供底层模块接口和实现
- 架构师B提供简化的高层API和端到端测试
- 两者可以共存，底层模块可被高层API调用
