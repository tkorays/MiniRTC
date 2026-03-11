# MiniRTC 统计模块设计

## 1. 设计目标

为MiniRTC实时通信库设计完整的统计模块，在Demo通话结束后打印完整的通话统计信息。设计参考WebRTC的Stats模型但做了适当简化，确保实现可行。

## 2. 统计数据结构设计

### 2.1 基础统计结构

```cpp
// 基础时间戳统计
struct RTCStatsMember {
    int64_t timestamp_ms = 0;           // 统计时间点
};

// 媒体类型枚举
enum class MediaType {
    kAudio,
    kVideo
};

// 统计ID类型
enum class RTCStatsType {
    kPeerConnection,         // 会话级别统计
    kTrack,                  // Track统计
    kSender,                 // 发送端统计  
    kReceiver,               // 接收端统计
    kTransport,              // 传输层统计
    kCodec                   // 编解码器统计
};
```

### 2.2 音频统计 (AudioSenderStats / AudioReceiverStats)

```cpp
// 音频发送端统计
struct AudioSenderStats : public RTCStatsMember {
    RTCStatsType type = RTCStatsType::kSender;
    uint32_t track_id = 0;
    std::string track_kind = "audio";
    
    // 编码统计
    uint64_t frames_encoded = 0;        // 已编码帧数
    uint64_t bytes_encoded = 0;          // 已编码字节数
    uint32_t encode_time_ms = 0;         // 编码耗时(毫秒)
    uint32_t encode_errors = 0;          // 编码错误次数
    
    // 发送统计
    uint64_t packets_sent = 0;           // 已发送RTP包数
    uint64_t bytes_sent = 0;             // 已发送字节数
    uint32_t ssrc = 0;                   // SSRC标识
    
    // 目标码率
    uint32_t target_bitrate_bps = 0;     // 目标码率(bps)
};

// 音频接收端统计
struct AudioReceiverStats : public RTCStatsMember {
    RTCStatsType type = RTCStatsType::kReceiver;
    uint32_t track_id = 0;
    std::string track_kind = "audio";
    
    // 接收统计
    uint64_t packets_received = 0;       // 已接收RTP包数
    uint64_t bytes_received = 0;         // 已接收字节数
    uint64_t packets_lost = 0;           // 丢包数
    uint32_t ssrc = 0;                   // SSRC标识
    
    // 解码统计
    uint64_t frames_decoded = 0;         // 已解码帧数(输出)
    uint64_t frames_rendered = 0;        // 已渲染帧数
    uint32_t decode_time_ms = 0;         // 解码耗时(毫秒)
    uint32_t decode_errors = 0;         // 解码错误次数
    
    // 抖动与延迟
    double jitter_ms = 0.0;              // 抖动(毫秒)
    uint32_t round_trip_time_ms = 0;     // RTT(毫秒)
    
    // 音频质量
    uint32_t sample_rate = 0;            // 采样率
    uint32_t channels = 0;               // 声道数
    float audio_level = 0.0;             // 音频电平(0-1)
    float total_audio_energy = 0.0;     // 总音频能量
    uint64_t total_samples_duration = 0;// 总音频样本时长
};
```

### 2.3 视频统计 (VideoSenderStats / VideoReceiverStats)

```cpp
// 视频发送端统计
struct VideoSenderStats : public RTCStatsMember {
    RTCStatsType type = RTCStatsType::kSender;
    uint32_t track_id = 0;
    std::string track_kind = "video";
    
    // 编码统计
    uint64_t frames_encoded = 0;         // 已编码帧数
    uint64_t bytes_encoded = 0;          // 已编码字节数
    uint32_t encode_time_ms = 0;         // 平均编码耗时(毫秒)
    uint32_t encode_errors = 0;          // 编码错误次数
    
    // 帧类型统计
    uint64_t key_frames_encoded = 0;     // 已编码关键帧(I帧)数
    uint64_t delta_frames_encoded = 0;   // 已编码Delta帧(P/B帧)数
    
    // 发送统计
    uint64_t packets_sent = 0;           // 已发送RTP包数
    uint64_t bytes_sent = 0;             // 已发送字节数
    uint32_t ssrc = 0;                   // SSRC标识
    
    // 码率控制
    uint32_t target_bitrate_bps = 0;     // 目标码率(bps)
    uint32_t actual_bitrate_bps = 0;     // 实际码率(bps)
    
    // 帧率
    double frame_rate_input = 0.0;       // 输入帧率
    double frame_rate_sent = 0.0;        // 发送帧率
    
    // 分辨率
    uint32_t frame_width = 0;            // 帧宽
    uint32_t frame_height = 0;           // 帧高
    uint32_t framesize_scale = 1;        // 编码缩放比例
};

// 视频接收端统计
struct VideoReceiverStats : public RTCStatsMember {
    RTCStatsType type = RTCStatsType::kReceiver;
    uint32_t track_id = 0;
    std::string track_kind = "video";
    
    // 接收统计
    uint64_t packets_received = 0;       // 已接收RTP包数
    uint64_t bytes_received = 0;         // 已接收字节数
    uint64_t packets_lost = 0;           // 丢包数
    uint32_t ssrc = 0;                   // SSRC标识
    
    // 解码统计
    uint64_t frames_decoded = 0;         // 已解码帧数
    uint64_t frames_rendered = 0;        // 已渲染帧数
    uint32_t decode_time_ms = 0;         // 平均解码耗时(毫秒)
    uint32_t decode_errors = 0;          // 解码错误次数
    uint64_t freeze_count = 0;           // 视频卡顿次数
    
    // 抖动与延迟
    double jitter_ms = 0.0;              // 抖动(毫秒)
    uint32_t round_trip_time_ms = 0;     // RTT(毫秒)
    
    // 分辨率
    uint32_t frame_width = 0;            // 帧宽(接收)
    uint32_t frame_height = 0;           // 帧高(接收)
    
    // 帧率
    double frame_rate_received = 0.0;    // 接收帧率
    double frame_rate_decoded = 0.0;     // 解码帧率
    double frame_rate_rendered = 0.0;    // 渲染帧率
};
```

### 2.4 连接/会话统计 (PeerConnectionStats)

```cpp
// ICECandidate统计
struct IceCandidatePairStats {
    std::string candidate_pair_id;
    std::string state;                    // Frozen/Waiting/InProgress/Success/Failed
    uint64_t packets_sent = 0;
    uint64_t packets_received = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t round_trip_time_ms = 0;     // RTT
    uint64_t available_outgoing_bitrate_bps = 0;
    uint64_t available_incoming_bitrate_bps = 0;
    uint32_t requests_received = 0;
    uint32_t requests_sent = 0;
    uint32_t responses_received = 0;
    uint32_t responses_sent = 0;
};

// 会话级别统计
struct PeerConnectionStats : public RTCStatsMember {
    RTCStatsType type = RTCStatsType::kPeerConnection;
    
    // 连接状态
    std::string state;                    // new/connecting/connected/disconnected/failed/closed
    uint64_t data_channels_opened = 0;
    uint64_t data_channels_closed = 0;
    
    // 绑定的传输ID
    std::string transport_id;
    
    // ICE候选对统计
    std::vector<IceCandidatePairStats> candidate_pair_stats;
    
    // 证书信息(如有)
    std::string local_certificate_id;
    std::string remote_certificate_id;
};
```

### 2.5 传输层统计 (TransportStats)

```cpp
// 传输层统计
struct TransportStats : public RTCStatsMember {
    RTCStatsType type = RTCStatsType::kTransport;
    
    std::string transport_id;
    std::string selected_candidate_pair_id;
    
    // SCTP/UDP统计
    uint64_t packets_sent = 0;
    uint64_t packets_received = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    
    // SRTP/SRTCP统计
    uint64_t srtp_packets_sent = 0;
    uint64_t srtp_packets_received = 0;
    uint64_t srtcp_packets_sent = 0;
    uint64_t srtcp_packets_received = 0;
    
    // 加密状态
    bool srtp_cipher_suite = false;
    bool srtcp_cipher_suite = false;
};
```

### 2.6 统一统计报告

```cpp
// 统计报告
struct RTCStatsReport {
    int64_t timestamp_ms = 0;            // 报告生成时间
    
    // 会话统计
    std::unique_ptr<PeerConnectionStats> peer_connection_stats;
    
    // 传输统计
    std::unique_ptr<TransportStats> transport_stats;
    
    // 发送端统计
    std::vector<AudioSenderStats> audio_sender_stats;
    std::vector<VideoSenderStats> video_sender_stats;
    
    // 接收端统计
    std::vector<AudioReceiverStats> audio_receiver_stats;
    std::vector<VideoReceiverStats> video_receiver_stats;
    
    // 会话持续时间
    uint64_t session_duration_ms = 0;
};
```

## 3. 各模块统计指标

### 3.1 音频模块统计指标

| 指标 | 发送端 | 接收端 | 说明 |
|------|--------|--------|------|
| frames_encoded/decoded | ✓ | ✓ | 编码/解码帧数 |
| bytes_encoded/received | ✓ | ✓ | 编码/接收字节数 |
| packets_sent/received | ✓ | ✓ | RTP包数 |
| packets_lost | - | ✓ | 丢包数 |
| encode/decode_time_ms | ✓ | ✓ | 编解码耗时 |
| encode/decode_errors | ✓ | ✓ | 错误次数 |
| jitter_ms | - | ✓ | 抖动 |
| round_trip_time_ms | ✓ | ✓ | RTT |
| sample_rate/channels | ✓ | ✓ | 采样率/声道数 |
| audio_level | - | ✓ | 音频电平 |

### 3.2 视频模块统计指标

| 指标 | 发送端 | 接收端 | 说明 |
|------|--------|--------|------|
| frames_encoded/decoded | ✓ | ✓ | 编码/解码帧数 |
| key_frames_encoded | ✓ | - | I帧数 |
| delta_frames_encoded | ✓ | - | P/B帧数 |
| bytes_encoded/received | ✓ | ✓ | 编码/接收字节数 |
| packets_sent/received | ✓ | ✓ | RTP包数 |
| packets_lost | - | ✓ | 丢包数 |
| frame_width/height | ✓ | ✓ | 分辨率 |
| frame_rate_* | ✓ | ✓ | 各种帧率 |
| target/actual_bitrate | ✓ | - | 码率 |
| jitter_ms | - | ✓ | 抖动 |
| freeze_count | - | ✓ | 卡顿次数 |

### 3.3 连接与传输统计指标

| 指标 | 说明 |
|------|------|
| state | 连接状态 |
| session_duration_ms | 会话持续时间 |
| packets_sent/received | 传输层包数 |
| bytes_sent/received | 传输层字节数 |
| round_trip_time_ms | RTT |
| available_bitrate_* | 可用带宽 |
| srtp_cipher_suite | SRTP加密套件 |

## 4. API接口设计

### 4.1 PeerConnection接口扩展

```cpp
class IPeerConnection {
public:
    // ... 现有接口 ...
    
    // 获取完整统计报告
    virtual std::unique_ptr<RTCStatsReport> GetStats() = 0;
    
    // 获取指定类型的统计
    virtual std::unique_ptr<AudioSenderStats> GetAudioSenderStats(uint32_t track_id) = 0;
    virtual std::unique_ptr<AudioReceiverStats> GetAudioReceiverStats(uint32_t track_id) = 0;
    virtual std::unique_ptr<VideoSenderStats> GetVideoSenderStats(uint32_t track_id) = 0;
    virtual std::unique_ptr<VideoReceiverStats> GetVideoReceiverStats(uint32_t track_id) = 0;
    
    // 获取传输统计
    virtual std::unique_ptr<TransportStats> GetTransportStats() = 0;
    
    // 获取会话持续时间
    virtual uint64_t GetSessionDurationMs() const = 0;
};
```

### 4.2 Track接口扩展

```cpp
class ITrack : public std::enable_shared_from_this<ITrack> {
public:
    // ... 现有接口 ...
    
    // 获取发送端统计
    virtual std::unique_ptr<AudioSenderStats> GetSenderStats() const = 0;
    virtual std::unique_ptr<VideoSenderStats> GetSenderStats() const = 0;
    
    // 获取接收端统计(需要远端反馈)
    virtual std::unique_ptr<AudioReceiverStats> GetReceiverStats() const = 0;
    virtual std::unique_ptr<VideoReceiverStats> GetReceiverStats() const = 0;
};
```

### 4.3 统计收集器接口

```cpp
// 统计收集器 - 负责在各模块中收集统计
class IStatsCollector {
public:
    using Ptr = std::shared_ptr<IStatsCollector>;
    virtual ~IStatsCollector() = default;
    
    // 初始化
    virtual void Initialize() = 0;
    
    // 更新统计
    virtual void Update() = 0;
    
    // 获取报告
    virtual std::unique_ptr<RTCStatsReport> GetReport() = 0;
    
    // 重置统计
    virtual void Reset() = 0;
};

// 编解码统计观察者
class ICodecStatsObserver {
public:
    virtual void OnEncodeComplete(const VideoFrame& frame, uint32_t encode_time_ms) = 0;
    virtual void OnDecodeComplete(const VideoFrame& frame, uint32_t decode_time_ms) = 0;
    virtual void OnEncodeError(ErrorCode error) = 0;
    virtual void OnDecodeError(ErrorCode error) = 0;
};

// 传输统计观察者
class ITransportStatsObserver {
public:
    virtual void OnPacketSent(const RtpPacket& packet, uint64_t bytes) = 0;
    virtual void OnPacketReceived(const RtpPacket& packet, uint64_t bytes) = 0;
    virtual void OnRttUpdated(uint32_t rtt_ms) = 0;
    virtual void OnBitrateUpdated(uint32_t send_bps, uint32_t recv_bps) = 0;
};
```

## 5. Demo输出格式示例

### 5.1 完整统计输出

```
============================================
       MiniRTC 通话统计报告
============================================
会话状态: Connected
持续时间: 120.5 秒

----------------[ 音频发送 ]----------------
  编码帧数:     6025
  编码字节:     4.82 MB
  编码耗时:     12 ms/帧 (平均)
  编码错误:     0
  发送包数:     6025
  发送字节:     4.82 MB
  目标码率:    32 kbps

----------------[ 音频接收 ]----------------
  接收包数:     6023
  接收字节:     4.81 MB
  丢包数:       2
  解码帧数:     6023
  解码耗时:     8 ms/帧 (平均)
  解码错误:     0
  采样率:       48000 Hz
  声道数:       2
  音频电平:     0.35
  抖动:         5.2 ms
  RTT:          38 ms

----------------[ 视频发送 ]----------------
  编码帧数:     3615
  I帧数:        120
  P帧数:        3495
  编码字节:     15.67 MB
  编码耗时:     28 ms/帧 (平均)
  编码错误:     0
  发送包数:     3615
  发送字节:     15.67 MB
  发送帧率:     30.0 fps
  分辨率:       1280 x 720
  目标码率:     1000 kbps
  实际码率:     1045 kbps

----------------[ 视频接收 ]----------------
  接收包数:     3612
  接收字节:     15.64 MB
  丢包数:       3
  解码帧数:     3612
  解码耗时:     15 ms/帧 (平均)
  解码错误:     0
  渲染帧数:     3612
  卡顿次数:     0
  抖动:         3.8 ms
  RTT:          38 ms
  接收帧率:     30.0 fps
  解码帧率:     30.0 fps
  渲染帧率:     30.0 fps
  分辨率:       1280 x 720

----------------[ 传输层 ]-----------------
  RTP发送包:    9640
  RTP接收包:    9635
  RTP发送字节: 20.49 MB
  RTP接收字节: 20.45 MB
  发送码率:    1360 kbps
  接收码率:    1352 kbps

----------------[ 丢包率 ]-----------------
  音频丢包率:   0.03%
  视频丢包率:   0.08%
  整体丢包率:  0.05%

============================================
```

### 5.2 简化版输出

```
========== MiniRTC 通话统计 ==========
连接状态: Connected
持续时间: 30.5 秒

[音频发送]
  编码帧: 1525
  字节数: 1.23 MB

[音频接收]
  解码帧: 1523
  字节数: 1.22 MB
  丢包: 2
  抖动: 12 ms
  RTT: 45 ms

[视频发送]
  编码帧: 915
  I帧: 30
  字节数: 4.56 MB
  分辨率: 640x480

[视频接收]
  解码帧: 912
  字节数: 4.51 MB
  丢包: 3
  分辨率: 640x480

[传输]
  发送bitrate: 1500 kbps
  接收bitrate: 1450 kbps
  RTT: 45 ms
  丢包率: 0.1%

======================================
```

## 6. 实现计划

### Phase 1: 基础结构
1. 在 `include/minirtc/` 下创建 `stats.h` 头文件，定义所有统计结构
2. 扩展 `TrackStats` 结构
3. 添加 `AudioSenderStats`, `AudioReceiverStats`, `VideoSenderStats`, `VideoReceiverStats`

### Phase 2: 接口实现
4. 扩展 `ITrack` 接口添加 `GetSenderStats/GetReceiverStats`
5. 扩展 `IPeerConnection` 接口添加 `GetStats`
6. 实现统计收集器 `StatsCollector`

### Phase 3: 模块集成
7. 在编码器/解码器中添加统计回调
8. 在RTP传输层添加包计数
9. 在RTCP模块中添加RTT计算

### Phase 4: Demo展示
10. 在Demo结束时调用 `GetStats()` 并打印统计报告

## 7. 文件修改清单

| 文件 | 修改内容 |
|------|----------|
| `include/minirtc/stats.h` | 新建，定义所有统计结构 |
| `include/minirtc/stream_track.h` | 扩展TrackStats，添加GetStats接口 |
| `include/minirtc/peer_connection.h` | 添加GetStats接口 |
| `src/stats_collector.cc` | 新建，实现统计收集器 |
| `src/peer_connection.cc` | 集成统计收集 |
| `src/stream_track.cc` | 实现Track级统计 |
| `examples/minirtc_demo.cc` | 添加统计打印代码 |

## 8. 实现注意事项

1. **线程安全**: 统计数据的读写需要在关键路径上，使用原子操作或锁保护
2. **性能影响**: 统计收集应尽量减少对主流程的性能影响，可采用采样或异步收集
3. **内存管理**: 统计报告应在需要时动态创建，避免长期占用大量内存
4. **精度与单位**: 统一使用毫秒、字节、帧等标准单位，避免歧义
5. **向后兼容**: 新增统计字段应保持默认值为0，不影响现有代码
