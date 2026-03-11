# MiniRTC API 参考

## 核心接口

### PeerConnection

#### IPeerConnection

通话连接管理的主接口。

```cpp
class IPeerConnection {
public:
    using Ptr = std::shared_ptr<IPeerConnection>;
    
    // 初始化
    virtual bool Initialize(const PeerConnectionConfig& config) = 0;
    
    // 设置回调
    virtual void SetHandler(std::shared_ptr<IPeerConnectionHandler> handler) = 0;
    
    // 添加本地Track
    virtual bool AddTrack(std::shared_ptr<ITrack> track) = 0;
    
    // 启动连接
    virtual bool Start() = 0;
    
    // 停止连接
    virtual void Stop() = 0;
    
    // 获取状态
    virtual PeerConnectionState GetState() = 0;
};
```

#### PeerConnectionConfig

```cpp
struct PeerConnectionConfig {
    std::vector<std::string> ice_servers = {
        "stun:stun.l.google.com:19302"
    };
    bool enable_audio = true;
    bool enable_video = true;
    uint32_t audio_bitrate_bps = 48000;
    uint32_t video_bitrate_bps = 500000;
};
```

### Stream/Track

#### ITrack

单一媒体轨道。

```cpp
class ITrack {
public:
    using Ptr = std::shared_ptr<ITrack>;
    
    virtual MediaKind GetKind() const = 0;
    virtual uint32_t GetId() const = 0;
    virtual std::string GetName() const = 0;
    virtual uint32_t GetSsrc() const = 0;
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() const = 0;
};
```

#### IStream

会话流。

```cpp
class IStream {
public:
    using Ptr = std::shared_ptr<IStream>;
    
    virtual bool AddTrack(ITrack::Ptr track) = 0;
    virtual bool RemoveTrack(uint32_t track_id) = 0;
    virtual std::vector<ITrack::Ptr> GetTracks() = 0;
    virtual bool Start() = 0;
    virtual void Stop() = 0;
};
```

### JitterBuffer

#### IJitterBuffer

```cpp
class IJitterBuffer {
public:
    using Ptr = std::shared_ptr<IJitterBuffer>;
    
    virtual void SetMode(JitterBufferMode mode) = 0;
    virtual void SetFixedDelay(int delay_ms) = 0;
    virtual bool Initialize(const JitterBufferConfig& config) = 0;
    virtual void AddPacket(std::shared_ptr<RtpPacket> packet) = 0;
    virtual std::shared_ptr<RtpPacket> GetPacket(int timeout_ms) = 0;
    virtual JitterBufferStats GetStats() const = 0;
};
```

### 带宽估计

#### IBandwidthEstimator

```cpp
class IBandwidthEstimator {
public:
    using Ptr = std::shared_ptr<IBandwidthEstimator>;
    
    virtual void Initialize(const BweConfig& config) = 0;
    virtual void OnPacketFeedback(const PacketFeedback& feedback) = 0;
    virtual void OnRttUpdate(int64_t rtt_ms) = 0;
    virtual BweResult GetResult() const = 0;
    virtual void Reset() = 0;
};
```

## 工厂函数

| 函数 | 说明 |
|------|------|
| `CreatePeerConnection()` | 创建PeerConnection实例 |
| `CreateStreamManager()` | 创建StreamManager实例 |
| `CreateJitterBuffer()` | 创建JitterBuffer实例 |
| `CreateBandwidthEstimator()` | 创建带宽估计器实例 |
| `CreateH264Packer()` | 创建H.264打包器 |
| `CreateVideoAssembler()` | 创建视频组帧器 |
| `CreateIceAgent()` | 创建ICE Agent |

## 错误处理

使用 `RtcError` 枚举：

```cpp
enum class RtcError {
    kOk = 0,
    kInvalidParam = -1,
    kNotInitialized = -2,
    kNotSupported = -4,
    kNoMemory = -5,
    kTimeout = -6,
    // ...
};
```

## 类型定义

| 类型 | 说明 |
|------|------|
| `MediaKind` | 媒体类型 (kAudio, kVideo) |
| `PeerConnectionState` | 连接状态 |
| `JitterBufferMode` | JitterBuffer模式 |
| `SrtpCryptoSuite` | SRTP加密套件 |

## 更多信息

- 查看 `test/integration/` 中的示例代码
- 查看 `include/minirtc/` 中的完整接口定义
