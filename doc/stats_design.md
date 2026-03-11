# MiniRTC 统计模块设计

## 1. 现有统计

```cpp
// TrackStats (当前)
struct TrackStats {
    uint64_t rtp_sent = 0;
    uint64_t rtp_received = 0;
};
```

## 2. 扩展设计

### 2.1 音频统计 (AudioStats)
```cpp
struct AudioStats {
    // 编码统计
    uint64_t frames_encoded = 0;
    uint64_t bytes_encoded = 0;
    uint64_t encode_errors = 0;
    
    // 解码统计
    uint64_t frames_decoded = 0;
    uint64_t bytes_decoded = 0;
    uint64_t decode_errors = 0;
    uint64_t packets_lost = 0;
    
    // 抖动
    uint64_t jitter_ms = 0;
    uint64_t latency_ms = 0;
};
```

### 2.2 视频统计 (VideoStats)
```cpp
struct VideoStats {
    // 编码统计
    uint64_t frames_encoded = 0;
    uint64_t bytes_encoded = 0;
    uint64_t encode_errors = 0;
    uint64_t keyframes = 0;  // I帧
    
    // 解码统计
    uint64_t frames_decoded = 0;
    uint64_t bytes_decoded = 0;
    uint64_t decode_errors = 0;
    uint64_t packets_lost = 0;
    
    // 分辨率
    uint32_t width = 0;
    uint32_t height = 0;
    
    // 抖动
    uint64_t jitter_ms = 0;
    uint64_t latency_ms = 0;
};
```

### 2.3 连接统计 (ConnectionStats)
```cpp
struct ConnectionStats {
    // 连接状态
    PeerConnectionState state = PeerConnectionState::kNew;
    
    // ICE
    std::string ice_state;
    std::string local_candidate;
    std::string remote_candidate;
    
    // 带宽 (kbps)
    uint64_t send_bitrate = 0;
    uint64_t recv_bitrate = 0;
    uint64_t target_bitrate = 0;
    
    // RTT (ms)
    uint64_t rtt_ms = 0;
    
    // 丢包率 (%)
    double send_loss_rate = 0.0;
    double recv_loss_rate = 0.0;
};
```

### 2.4 传输统计 (TransportStats)
```cpp
struct TransportStats {
    // RTP
    uint64_t rtp_sent = 0;
    uint64_t rtp_received = 0;
    uint64_t rtp_lost = 0;
    
    // RTCP
    uint64_t rtcp_sent = 0;
    uint64_t rtcp_received = 0;
    
    // 字节
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
};
```

## 3. 统一统计接口

### RTCStats
```cpp
struct RTCStats {
    ConnectionStats connection;
    TransportStats transport;
    std::vector<AudioStats> audio_stats;
    std::vector<VideoStats> video_stats;
    
    // 时间戳
    uint64_t timestamp_ms = 0;
};
```

### 获取统计接口
```cpp
class IPeerConnection {
public:
    virtual RTCStats GetStats() = 0;
};

class ITrack {
public:
    virtual AudioStats GetAudioStats() = 0;
    virtual VideoStats GetVideoStats() = 0;
    virtual TrackStats GetTrackStats() = 0;
};
```

## 4. Demo输出示例

```
========== MiniRTC 通话统计 ==========
连接状态: Connected
持续时间: 30.5 秒

[音频发送]
  编码帧: 1525
  字节数: 1.23 MB
  丢包: 0

[音频接收]
  解码帧: 1523
  字节数: 1.22 MB
  丢包: 2
  抖动: 12 ms

[视频发送]
  编码帧: 915
  I帧: 30
  字节数: 4.56 MB

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

## 5. 实现计划

1. 扩展 TrackStats 结构
2. 添加 ConnectionStats/TransportStats
3. 在 PeerConnection/Track 中收集统计
4. Demo结束时打印完整统计

## 6. 文件修改

- include/minirtc/stream_track.h - 扩展统计结构
- include/minirtc/peer_connection.h - 添加GetStats接口
- src/peer_connection.cc - 实现统计收集
- src/stream_track.cc - 实现统计收集
- examples/minirtc_demo.cc - 打印统计
