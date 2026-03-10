# MiniRTC 传输层接口设计文档 v3.0

**版本**: 3.0
**日期**: 2026-03-10
**状态**: 初稿

---

## 1. 概述

本文档描述MiniRTC v3.0传输层的详细接口设计。传输层负责RTP/RTCP数据的发送和接收，是媒体数据从应用到网络的桥梁。传输层设计遵循以下原则：

- **模块化**: 清晰的接口定义，便于扩展和替换
- **面向对象**: 使用C++抽象基类实现多态
- **异步处理**: 支持异步发送和接收
- **统计完备**: 内置丰富的传输统计功能

---

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           传输层架构                                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                      ITransport (基类)                          │   │
│  │  ┌─────────────────────────────────────────────────────────┐   │   │
│  │  │ + Send() / Receive() / Close() / GetStats()            │   │   │
│  │  └─────────────────────────────────────────────────────────┘   │   │
│  └──────────────────────────┬──────────────────────────────────────┘   │
│                             │                                           │
│         ┌───────────────────┼───────────────────┐                     │
│         ▼                   ▼                   ▼                     │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐                │
│  │RTPTransport │    │SRTPTransport│    │NetworkInterf│                │
│  │             │    │   (预留)    │    │    ace      │                │
│  └──────┬──────┘    └─────────────┘    └──────┬──────┘                │
│         │                                      │                        │
│         └──────────────────┬─────────────────┘                        │
│                            ▼                                           │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                      RTCPModule                                  │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │   │
│  │  │  SR        │  │   RR        │  │   NACK      │              │   │
│  │  │ (Sender)   │  │ (Receiver)  │  │  (Feedback) │              │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘              │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                            │                                           │
│                            ▼                                           │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    ICE / DTLS / SDP (预留)                      │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │   │
│  │  │    ICE     │  │    DTLS    │  │    SDP      │              │   │
│  │  │  (穿透)    │  │  (加密)    │  │  (协商)     │              │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘              │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. ITransport 基类设计

### 3.1 设计原则

ITransport是所有传输实现的抽象基类，定义通用的传输操作接口。子类只需实现平台相关的网络操作，通用逻辑在基类中实现。

### 3.2 核心类型定义

```cpp
namespace minirtc {

/// 传输错误码
enum class TransportError {
  kOk = 0,                    // 成功
  kNotInitialized,            // 未初始化
  kInvalidParam,               // 参数错误
  kSocketError,                // Socket错误
  kTimeout,                    // 超时
  kBufferOverflow,             // 缓冲区溢出
  kConnectionClosed,           // 连接已关闭
  kNotSupported,               // 不支持的操作
};

/// 传输类型
enum class TransportType {
  kUdp,                       // UDP传输
  kTcp,                       // TCP传输
  kSrtp,                      // SRTP传输 (预留)
};

/// 传输状态
enum class TransportState {
  kClosed,                    // 关闭
  kOpening,                   // 打开中
  kOpen,                      // 已打开
  kError,                     // 错误
};

/// 网络地址
struct NetworkAddress {
  std::string ip;             // IP地址
  uint16_t port;              // 端口号
  
  bool operator==(const NetworkAddress& other) const {
    return ip == other.ip && port == other.port;
  }
};

/// 传输统计信息
struct TransportStats {
  uint64_t packets_sent;              // 已发送包数
  uint64_t packets_received;          // 已接收包数
  uint64_t bytes_sent;                // 已发送字节数
  uint64_t bytes_received;            // 已接收字节数
  uint64_t packets_lost;             // 丢包数
  uint32_t round_trip_time_ms;        // RTT (毫秒)
  float jitter_ms;                    // 抖动 (毫秒)
  uint32_t sender_bitrate_bps;         // 发送码率 (bps)
  uint32_t receiver_bitrate_bps;       // 接收码率 (bps)
  uint64_t last_packet_timestamp_us;  // 最后包时间戳 (微秒)
};

/// 传输配置
struct TransportConfig {
  TransportType type = TransportType::kUdp;
  NetworkAddress local_addr;            // 本地地址
  NetworkAddress remote_addr;           // 远端地址
  uint32_t socket_buffer_size = 65536; // Socket缓冲区大小
  bool enable_ipv6 = false;             // 是否支持IPv6
  int timeout_ms = 3000;               // 超时时间 (毫秒)
};

}  // namespace minirtc
```

### 3.3 ITransport 接口定义

```cpp
namespace minirtc {

/// RTP数据包
class RtpPacket {
 public:
  virtual ~RtpPacket() = default;
  
  // RTP头字段
  virtual uint16_t GetSequenceNumber() const = 0;
  virtual void SetSequenceNumber(uint16_t seq) = 0;
  
  virtual uint32_t GetTimestamp() const = 0;
  virtual void SetTimestamp(uint32_t ts) = 0;
  
  virtual uint32_t GetSsrc() const = 0;
  virtual void SetSsrc(uint32_t ssrc) = 0;
  
  virtual uint8_t GetPayloadType() const = 0;
  virtual void SetPayloadType(uint8_t pt) = 0;
  
  virtual uint8_t GetMarker() const = 0;
  virtual void SetMarker(uint8_t m) = 0;
  
  // 载荷操作
  virtual const uint8_t* GetPayload() const = 0;
  virtual uint16_t GetPayloadSize() const = 0;
  virtual int SetPayload(const uint8_t* data, uint16_t size) = 0;
  
  // 完整包数据
  virtual const uint8_t* GetData() const = 0;
  virtual uint16_t GetSize() const = 0;
  
  // 扩展头
  virtual bool HasExtension() const = 0;
  virtual const uint8_t* GetExtensionData() const = 0;
  virtual uint16_t GetExtensionSize() const = 0;
};

/// 传输回调接口
class ITransportCallback {
 public:
  virtual ~ITransportCallback() = default;
  
  /// RTP包接收回调
  virtual void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet,
                                   const NetworkAddress& from) = 0;
  
  /// 传输错误回调
  virtual void OnTransportError(TransportError error, const std::string& message) = 0;
  
  /// 状态变化回调
  virtual void OnTransportStateChanged(TransportState state) = 0;
};

/// 传输基类接口
class ITransport {
 public:
  virtual ~ITransport() = default;
  
  /// 初始化传输
  virtual TransportError Open(const TransportConfig& config) = 0;
  
  /// 关闭传输
  virtual void Close() = 0;
  
  /// 获取传输状态
  virtual TransportState GetState() const = 0;
  
  /// 获取传输类型
  virtual TransportType GetType() const = 0;
  
  /// 发送RTP包
  virtual TransportError SendRtpPacket(std::shared_ptr<RtpPacket> packet) = 0;
  
  /// 接收RTP包 (阻塞模式)
  virtual TransportError ReceiveRtpPacket(std::shared_ptr<RtpPacket>* packet,
                                          NetworkAddress* from,
                                          int timeout_ms) = 0;
  
  /// 设置回调
  virtual void SetCallback(ITransportCallback* callback) = 0;
  
  /// 获取本地地址
  virtual const NetworkAddress& GetLocalAddress() const = 0;
  
  /// 获取远端地址
  virtual const NetworkAddress& GetRemoteAddress() const = 0;
  
  /// 获取传输统计
  virtual TransportStats GetStats() const = 0;
  
  /// 重置统计
  virtual void ResetStats() = 0;
};

}  // namespace minirtc
```

---

## 4. RTPTransport 接口设计

### 4.1 设计概述

RTPTransport实现标准的RTP传输功能，支持UDP和TCP两种传输模式。它继承自ITransport，提供可靠的RTP包发送和接收能力。

### 4.2 核心接口定义

```cpp
namespace minirtc {

/// RTP传输配置
struct RtpTransportConfig : public TransportConfig {
  uint32_t ssrc = 0;                    // SSRC标识
  uint16_t rtcp_port = 0;              // RTCP端口 (偶数+1)
  bool enable_rtcp = true;             // 是否启用RTCP
  bool enable_nack = false;             // 是否启用NACK
  bool enable_fec = false;              // 是否启用FEC
  int max_packet_size = 1500;           // 最大包大小
  bool enable_rtx = false;              // 是否启用重传
  uint8_t rtx_payload_type = 0;         // RTX载荷类型
};

/// RTP传输事件回调
class IRtpTransportCallback : public ITransportCallback {
 public:
  /// RTX包接收回调 (重传包)
  virtual void OnRtxPacketReceived(std::shared_ptr<RtpPacket> packet,
                                    const NetworkAddress& from) = 0;
  
  /// FEC包接收回调
  virtual void OnFecPacketReceived(std::shared_ptr<RtpPacket> packet,
                                    const NetworkAddress& from) = 0;
};

/// RTP传输接口
class IRTPTransport : public ITransport {
 public:
  virtual ~IRTPTransport() = default;
  
  /// 设置RTP传输配置
  virtual TransportError SetConfig(const RtpTransportConfig& config) = 0;
  
  /// 获取RTP传输配置
  virtual RtpTransportConfig GetConfig() const = 0;
  
  /// 发送RTP数据 (原始数据接口)
  virtual TransportError SendRtpData(const uint8_t* data,
                                       size_t size,
                                       uint8_t payload_type,
                                       uint32_t timestamp,
                                       bool marker) = 0;
  
  /// 发送RTX重传包
  virtual TransportError SendRtxPacket(uint16_t original_seq,
                                        const uint8_t* data,
                                        size_t size) = 0;
  
  /// 设置远端地址 (用于UDP模式)
  virtual TransportError SetRemoteAddress(const NetworkAddress& addr) = 0;
  
  /// 添加远端地址候选 (用于ICE)
  virtual void AddRemoteCandidate(const NetworkAddress& addr) = 0;
  
  /// 清除远端地址候选
  virtual void ClearRemoteCandidates() = 0;
  
  /// 启动接收循环
  virtual void StartReceiving() = 0;
  
  /// 停止接收循环
  virtual void StopReceiving() = 0;
  
  /// 获取RTP接收统计
  virtual RtpReceiveStats GetRtpReceiveStats() const = 0;
  
  /// 获取RTP发送统计
  virtual RtpSendStats GetRtpSendStats() const = 0;
};

/// RTP接收统计
struct RtpReceiveStats {
  uint64_t total_packets;               // 总包数
  uint64_t total_bytes;                 // 总字节数
  uint16_t last_seq;                    // 最新序列号
  uint16_t cycles;                      // 序列号循环次数
  uint64_t packets_lost;                // 丢包数
  float fraction_lost;                  // 丢包率
  uint32_t jitter;                      // 抖动 (RTP时间戳单位)
  uint64_t last_arrival_time_us;        // 最后到达时间
};

/// RTP发送统计
struct RtpSendStats {
  uint64_t total_packets;               // 总包数
  uint64_t total_bytes;                 // 总字节数
  uint16_t next_seq;                    // 下一序列号
  uint32_t timestamp;                   // 当前时间戳
  uint32_t bitrate_bps;                 // 当前码率
};

}  // namespace minirtc
```

### 4.3 使用示例

```cpp
// 创建RTPTransport实例
auto rtp_transport = std::make_unique<RtpTransport>();

// 配置
RtpTransportConfig config;
config.type = TransportType::kUdp;
config.local_addr = {"0.0.0.0", 5000};
config.remote_addr = {"192.168.1.100", 5000};
config.ssrc = 0x12345678;
config.enable_rtcp = true;

// 设置回调
class MyCallback : public IRtpTransportCallback {
  void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet,
                          const NetworkAddress& from) override {
    // 处理收到的RTP包
  }
  void OnTransportError(TransportError error, const std::string& msg) override {
    // 处理错误
  }
};
MyCallback callback;
rtp_transport->SetCallback(&callback);

// 打开传输
auto error = rtp_transport->Open(config);
if (error != TransportError::kOk) {
  // 处理错误
}

// 发送RTP包
rtp_transport->SendRtpData(data, size, 96, timestamp, true);

// 获取统计
auto stats = rtp_transport->GetStats();
```

---

## 5. RTCPModule 接口设计

### 5.1 设计概述

RTCPModule负责RTCP (RTP Control Protocol) 协议的实现，包括：
- SR (Sender Report): 发送者报告
- RR (Receiver Report): 接收者报告  
- SDES (Source Description): 源描述
- BYE: 会话结束
- APP: 应用自定义
- NACK/FB: 反馈消息 (用于NACK/FEC)

### 5.2 RTCP类型定义

```cpp
namespace minirtc {

/// RTCP包类型
enum class RtcpPacketType {
  kSR = 200,       // Sender Report
  kRR = 201,       // Receiver Report
  kSDES = 202,     // Source Description
  kBYE = 203,      // Goodbye
  kAPP = 204,      // Application Defined
  kRTPFB = 205,    // Transport Layer Feedback (NACK)
  kPSFB = 206,     // Payload-Specific Feedback (FEC)
};

/// 报告块
struct RtcpReportBlock {
  uint32_t ssrc;                    // 同步源SSRC
  uint8_t fraction_lost;           // 丢包率 (8位)
  int32_t packets_lost;            // 累计丢包数 (24位)
  uint32_t highest_seq;            // 最高序列号
  uint32_t jitter;                 // 抖动
  uint32_t lsr;                    // 最后SR时间戳
  uint32_t dlsr;                   // 最后SR延迟
};

/// RTCP Compound Packet 组合包
class RtcpPacket {
 public:
  virtual ~RtcpPacket() = default;
  
  /// 获取包类型
  virtual RtcpPacketType GetType() const = 0;
  
  /// 获取SSRC
  virtual uint32_t GetSsrc() const = 0;
  
  /// 序列化到缓冲区
  virtual int Serialize(uint8_t* buffer, size_t size) const = 0;
  
  /// 反序列化
  virtual int Deserialize(const uint8_t* data, size_t size) = 0;
  
  /// 获取包大小
  virtual size_t GetSize() const = 0;
};

/// Sender Report
class RtcpSrPacket : public RtcpPacket {
 public:
  virtual ~RtcpSrPacket() = default;
  
  virtual RtcpPacketType GetType() const override { return RtcpPacketType::kSR; }
  
  virtual uint32_t GetNtpTimestampHigh() const = 0;
  virtual uint32_t GetNtpTimestampLow() const = 0;
  virtual uint32_t GetRtpTimestamp() const = 0;
  virtual uint32_t GetPacketCount() const = 0;
  virtual uint32_t GetOctetCount() const = 0;
  
  virtual void SetNtpTimestamp(uint32_t high, uint32_t low) = 0;
  virtual void SetRtpTimestamp(uint32_t ts) = 0;
  virtual void SetPacketCount(uint32_t count) = 0;
  virtual void SetOctetCount(uint32_t count) = 0;
  
  virtual std::vector<RtcpReportBlock> GetReportBlocks() const = 0;
  virtual void AddReportBlock(const RtcpReportBlock& block) = 0;
};

/// Receiver Report
class RtcpRrPacket : public RtcpPacket {
 public:
  virtual ~RtcpRrPacket() override = default;
  
  virtual RtcpPacketType GetType() const override { return RtcpPacketType::kRR; }
  
  virtual std::vector<RtcpReportBlock> GetReportBlocks() const = 0;
  virtual void AddReportBlock(const RtcpReportBlock& block) = 0;
};

/// NACK Feedback (RTPFB)
class RtcpNackPacket : public RtcpPacket {
 public:
  virtual ~RtcpNackPacket() override = default;
  
  virtual RtcpPacketType GetType() const override { return RtcpPacketType::kRTPFB; }
  
  virtual uint32_t GetMediaSsrc() const = 0;
  virtual void SetMediaSsrc(uint32_t ssrc) = 0;
  
  virtual std::vector<uint16_t> GetNackList() const = 0;
  virtual void AddNack(uint16_t seq) = 0;
};

}  // namespace minirtc
```

### 5.3 RTCPModule 接口定义

```cpp
namespace minirtc {

/// RTCP配置
struct RtcpConfig {
  bool enable = true;                        // 是否启用RTCP
  bool enable_sr = true;                     // 是否发送SR
  bool enable_rr = true;                     // 是否发送RR
  bool enable_sdes = true;                   // 是否发送SDES
  bool enable_nack = false;                  // 是否启用NACK反馈
  bool enable_fb = false;                    // 是否启用反馈消息
  
  uint32_t interval_sr_ms = 5000;           // SR发送间隔 (毫秒)
  uint32_t interval_rr_ms = 5000;           // RR发送间隔 (毫秒)
  uint32_t interval_sdes_ms = 10000;        // SDES发送间隔 (毫秒)
  
  std::string cname;                        // CNAME (用于SDES)
  std::string name;                         // 用户名 (用于SDES)
  
  size_t max_report_blocks = 31;             // 最大报告块数
};

/// RTCP统计信息
struct RtcpStats {
  uint64_t sr_sent;                         // 已发送SR数
  uint64_t rr_sent;                         // 已发送RR数
  uint64_t sr_received;                    // 已接收SR数
  uint64_t rr_received;                     // 已接收RR数
  uint64_t nack_sent;                       // 已发送NACK数
  uint64_t nack_received;                   // 已接收NACK数
  
  uint64_t last_sr_timestamp;               // 最后SR的NTP时间戳
  uint64_t last_sr_time_us;                 // 最后SR的本地时间
  uint32_t avg_rtt_ms;                      // 平均RTT (毫秒)
  
  // 接收统计
  std::vector<RtcpReportBlock> last_report_blocks;  // 上次报告块
};

/// RTCP回调接口
class IRtcpCallback {
 public:
  virtual ~IRtcpCallback() = default;
  
  /// SR接收回调
  virtual void OnSenderReportReceived(uint32_t ssrc,
                                       uint64_t ntp_timestamp,
                                       uint32_t rtp_timestamp,
                                       uint32_t packet_count,
                                       uint32_t octet_count) = 0;
  
  /// RR接收回调
  virtual void OnReceiverReportReceived(uint32_t ssrc,
                                         const std::vector<RtcpReportBlock>& blocks) = 0;
  
  /// NACK请求回调
  virtual void OnNackRequested(const std::vector<uint16_t>& seq_nums) = 0;
  
  /// BYE接收回调
  virtual void OnByeReceived(uint32_t ssrc) = 0;
  
  /// 统计更新回调
  virtual void OnRtcpStatsUpdated(const RtcpStats& stats) = 0;
};

/// RTCP模块接口
class IRTCPModule {
 public:
  virtual ~IRTCPModule() = default;
  
  /// 初始化
  virtual void Initialize(uint32_t local_ssrc,
                         uint32_t remote_ssrc,
                         const RtcpConfig& config) = 0;
  
  /// 设置回调
  virtual void SetCallback(IRtcpCallback* callback) = 0;
  
  /// 绑定RTPTransport
  virtual void BindTransport(IRTPTransport* transport) = 0;
  
  /// 启动
  virtual void Start() = 0;
  
  /// 停止
  virtual void Stop() = 0;
  
  /// 发送Sender Report
  virtual void SendSr() = 0;
  
  /// 发送Receiver Report
  virtual void SendRr() = 0;
  
  /// 发送NACK (反馈)
  virtual void SendNack(const std::vector<uint16_t>& seq_nums) = 0;
  
  /// 发送BYE
  virtual void SendBye(const std::string& reason = "") = 0;
  
  /// 处理接收到的RTCP包
  virtual void OnRtcpPacketReceived(const uint8_t* data, size_t size,
                                     const NetworkAddress& from) = 0;
  
  /// 更新发送统计 (由RTPTransport调用)
  virtual void UpdateSenderStats(uint32_t packet_count,
                                  uint32_t octet_count,
                                  uint32_t timestamp) = 0;
  
  /// 更新接收统计 (由RTPTransport调用)
  virtual void UpdateReceiverStats(uint16_t seq, uint32_t arrival_time_ms) = 0;
  
  /// 获取RTCP统计
  virtual RtcpStats GetStats() const = 0;
  
  /// 设置配置
  virtual void SetConfig(const RtcpConfig& config) = 0;
  
  /// 获取配置
  virtual RtcpConfig GetConfig() const = 0;
};

}  // namespace minirtc
```

---

## 6. SRTPTransport (预留) 接口设计

### 6.1 设计概述

SRTP (Secure RTP) 是RTP的安全扩展，提供加密、认证和重放保护。该模块为预留状态，待DTLS实现后对接。

### 6.2 预留接口定义

```cpp
namespace minirtc {

/// SRTP加密算法
enum class SrtpCryptoSuite {
  kAesCm128HmacSha1_80,   // AES-CM with HMAC-SHA1 (80-bit tag)
  kAesCm128HmacSha1_32,   // AES-CM with HMAC-SHA1 (32-bit tag)
  kAesGcm128,             // AES-GCM (128-bit)
  kAesGcm256,             // AES-GCM (256-bit)
};

/// SRTP策略
struct SrtpPolicy {
  SrtpCryptoSuite send_suite = SrtpCryptoSuite::kAesCm128HmacSha1_80;
  SrtpCryptoSuite recv_suite = SrtpCryptoSuite::kAesCm128HmacSha1_80;
  
  // 密钥 (由DTLS提供)
  std::vector<uint8_t> send_key;
  std::vector<uint8_t> recv_key;
  
  // 重放保护窗口大小
  uint32_t replay_window_size = 128;
};

/// SRTP统计
struct SrtpStats {
  uint64_t send_encrypted_packets;     // 加密发送包数
  uint64_t recv_decrypted_packets;     // 解密接收包数
  uint64_t send_encryption_failures;   // 加密失败数
  uint64_t recv_decryption_failures;   // 解密失败数
  uint64_t recv_auth_failures;         // 认证失败数
  uint64_t recv_replay_failures;       // 重放攻击检测数
};

/// SRTP传输接口 (预留)
class ISRTPTransport : public IRTPTransport {
 public:
  virtual ~ISRTPTransport() = default;
  
  /// 设置SRTP策略
  virtual TransportError SetSrtpPolicy(const SrtpPolicy& policy) = 0;
  
  /// 获取SRTP策略
  virtual SrtpPolicy GetSrtpPolicy() const = 0;
  
  /// 更新密钥 (用于密钥轮换)
  virtual TransportError UpdateKeys(const std::vector<uint8_t>& send_key,
                                     const std::vector<uint8_t>& recv_key) = 0;
  
  /// 获取SRTP统计
  virtual SrtpStats GetSrtpStats() const = 0;
  
  /// 设置DTLS握手回调 (预留)
  virtual void SetDtlsHandler(void* handler) = 0;
};

}  // namespace minirtc
```

---

## 7. NetworkInterface 接口设计

### 7.1 设计概述

NetworkInterface是底层网络操作的抽象，提供统一的网络读写接口。它屏蔽了不同平台(Windows/Linux/macOS/Android/iOS)的网络API差异。

### 7.2 接口定义

```cpp
namespace minirtc {

/// 网络接口类型
enum class NetworkInterfaceType {
  kUdpSocket,
  kTcpSocket,
  kLoopback,       // 回路测试用
};

/// Socket选项
struct SocketOptions {
  bool reuse_addr = true;        // 地址复用
  bool reuse_port = false;       // 端口复用
  bool non_blocking = false;     // 非阻塞模式
  bool broadcast = false;         // 广播
  int send_buffer_size = 0;      // 发送缓冲区 (0=系统默认)
  int recv_buffer_size = 0;      // 接收缓冲区 (0=系统默认)
  int ttl = 128;                 // TTL
  int tos = 0;                  // Type of Service
};

/// 网络接口回调
class INetworkCallback {
 public:
  virtual ~INetworkCallback() = default;
  
  /// 数据接收回调
  virtual void OnDataReceived(const uint8_t* data,
                              size_t size,
                              const NetworkAddress& from) = 0;
  
  /// 错误回调
  virtual void OnError(TransportError error, const std::string& message) = 0;
};

/// 网络接口接口
class INetworkInterface {
 public:
  virtual ~INetworkInterface() = default;
  
  /// 创建Socket
  virtual TransportError Create(NetworkInterfaceType type,
                                const NetworkAddress& local_addr) = 0;
  
  /// 关闭Socket
  virtual void Close() = 0;
  
  /// 绑定地址
  virtual TransportError Bind(const NetworkAddress& addr) = 0;
  
  /// 连接 (用于TCP)
  virtual TransportError Connect(const NetworkAddress& addr) = 0;
  
  /// 监听 (用于TCP Server)
  virtual TransportError Listen(int backlog) = 0;
  
  /// 接受连接 (用于TCP Server)
  virtual TransportError Accept(INetworkInterface** client_socket,
                                NetworkAddress* client_addr) = 0;
  
  /// 发送数据
  virtual TransportError SendTo(const uint8_t* data,
                                size_t size,
                                const NetworkAddress& to) = 0;
  
  /// 接收数据
  virtual TransportError ReceiveFrom(uint8_t* buffer,
                                      size_t buffer_size,
                                      size_t* received,
                                      NetworkAddress* from) = 0;
  
  /// 设置Socket选项
  virtual TransportError SetOptions(const SocketOptions& options) = 0;
  
  /// 获取本地地址
  virtual NetworkAddress GetLocalAddress() const = 0;
  
  /// 获取远端地址
  virtual NetworkAddress GetRemoteAddress() const = 0;
  
  /// 设置回调
  virtual void SetCallback(INetworkCallback* callback) = 0;
  
  /// 设置非阻塞模式
  virtual TransportError SetNonBlocking(bool enabled) = 0;
  
  /// 获取Socket句柄
  virtual int GetSocketFd() const = 0;
  
  /// 获取接口类型
  virtual NetworkInterfaceType GetType() const = 0;
  
  /// 检查Socket是否有效
  virtual bool IsValid() const = 0;
};

}  // namespace minirtc
```

### 7.3 多网卡支持

```cpp
/// 网络接口管理器
class INetworkInterfaceManager {
 public:
  virtual ~INetworkInterfaceManager() = default;
  
  /// 获取所有可用网卡
  virtual std::vector<NetworkInterfaceInfo> GetInterfaces() = 0;
  
  /// 根据名称获取网卡
  virtual std::shared_ptr<INetworkInterface> GetInterface(const std::string& name) = 0;
  
  /// 创建指定网卡的Socket
  virtual std::shared_ptr<INetworkInterface> CreateSocket(
      const std::string& interface_name,
      NetworkInterfaceType type,
      const NetworkAddress& local_addr) = 0;
};

/// 网卡信息
struct NetworkInterfaceInfo {
  std::string name;                   // 网卡名称 (e.g., "eth0", "en0")
  std::string display_name;          // 显示名称
  std::vector<NetworkAddress> addresses;  // IP地址列表
  bool is_loopback;                   // 是否回路
  bool is_up;                         // 是否启用
  bool is_ipv6;                       // 是否支持IPv6
};
```

---

## 8. ICE/DTLS/SDP 预留接口设计

### 8.1 ICE 接口设计

ICE (Interactive Connectivity Establishment) 用于NAT穿透，支持STUN和TURN协议。

```cpp
namespace minirtc {

/// ICE候选类型
enum class IceCandidateType {
  kHost,       // 主机候选
  kSrflx,      // 服务器反射候选 (STUN)
  kPrflx,      // 对等反射候选
  kRelayed,    // 中继候选 (TURN)
};

/// ICE候选传输协议
enum class IceCandidateTransport {
  kUdp,
  kTcp,
  kTls,
};

/// ICE组件ID
enum class IceComponent : uint8_t {
  kRtp = 1,
  kRtcp = 2,
};

/// ICE候选
struct IceCandidate {
  uint32_t foundation;                 // 基础标识
  IceComponent component;              // 组件ID
  IceCandidateTransport transport;     // 传输协议
  IceCandidateType type;               // 候选类型
  uint32_t priority;                  // 优先级
  NetworkAddress address;              // 地址
  std::string username;                // 用户名 (用于TURN)
  std::string password;                // 密码 (用于TURN)
  
  std::string ToString() const;        // 转换为SDP格式
  static IceCandidate FromString(const std::string& str);  // 从SDP解析
};

/// ICE角色
enum class IceRole {
  kControlling,   // 控制角色
  kControlled,    // 被控制角色
};

/// ICE状态
enum class IceState {
  kNew,
  kChecking,
  kConnected,
  kCompleted,
  kFailed,
  kDisconnected,
  kClosed,
};

/// ICE配置
struct IceConfig {
  std::vector<std::string> stun_servers;    // STUN服务器列表
  std::vector<std::string> turn_servers;   // TURN服务器列表
  std::string username;                    // TURN用户名
  std::string password;                    // TURN密码
  
  int ice_timeout_ms = 25000;               // ICE超时
  int candidate_timeout_ms = 2000;         // 候选超时
  
  bool ice_lite = false;                   // ICE-Lite模式
};

/// ICE回调接口
class IIceCallback {
 public:
  virtual ~IIceCallback() = default;
  
  /// 候选收集完成
  virtual void OnIceCandidatesGathered(IceComponent component,
                                       const std::vector<IceCandidate>& candidates) = 0;
  
  /// ICE状态变化
  virtual void OnIceStateChanged(IceComponent component, IceState state) = 0;
  
  /// 连接成功
  virtual void OnIceConnectionConnected(IceComponent component) = 0;
  
  /// 连接失败
  virtual void OnIceConnectionFailed(IceComponent component,
                                     const std::string& reason) = 0;
  
  /// 收到候选
  virtual void OnIceCandidateReceived(IceComponent component,
                                       const IceCandidate& candidate) = 0;
};

/// ICE接口
class IIceTransport {
 public:
  virtual ~IIceTransport() = default;
  
  /// 初始化
  virtual void Initialize(const IceConfig& config,
                         IceRole role,
                         const std::string& tid) = 0;
  
  /// 设置回调
  virtual void SetCallback(IIceCallback* callback) = 0;
  
  /// 开始候选收集
  virtual void StartGathering() = 0;
  
  /// 添加远程候选
  virtual void AddRemoteCandidate(const IceCandidate& candidate) = 0;
  
  /// 设置远程SDP
  virtual void SetRemoteDescription(const std::string& sdp,
                                    const std::string& type) = 0;
  
  /// 获取本地SDP
  virtual std::string GetLocalDescription() = 0;
  
  /// 开始连接检查
  virtual void StartConnectivityChecks() = 0;
  
  /// 获取状态
  virtual IceState GetState(IceComponent component) const = 0;
  
  /// 获取选中候选
  virtual IceCandidate GetSelectedCandidate(IceComponent component) const = 0;
  
  /// 关闭
  virtual void Close() = 0;
};

}  // namespace minirtc
```

### 8.2 DTLS 接口设计

DTLS (Datagram Transport Layer Security) 用于密钥交换和数据加密。

```cpp
namespace minirtc {

/// DTLS角色
enum class DtlsRole {
  kClient,
  kServer,
};

/// DTLS状态
enum class DtlsState {
  kNew,
  kConnecting,
  kConnected,
  kFailed,
  kClosed,
};

/// DTLS指纹 (用于SDP)
struct DtlsFingerprint {
  std::string algorithm;      // e.g., "SHA-256"
  std::string fingerprint;   // 证书指纹
};

/// DTLS配置
struct DtlsConfig {
  DtlsRole role = DtlsRole::kClient;
  
  // 证书 (PEM格式)
  std::string certificate;
  std::string private_key;
  
  // SRTP预主密钥导出
  bool srtp_profiles = true;
  
  // DTLS扩展
  bool enable_srtp = true;
  bool enable_renegotiation = false;
};

/// DTLS回调接口
class IDtlsCallback {
 public:
  virtual ~IDtlsCallback() = default;
  
  /// 握手完成
  virtual void OnDtlsHandshakeComplete() = 0;
  
  /// 握手失败
  virtual void OnDtlsHandshakeFailed(const std::string& reason) = 0;
  
  /// 状态变化
  virtual void OnDtlsStateChanged(DtlsState state) = 0;
  
  /// SRTP密钥就绪
  virtual void OnSrtpKeysReady(const std::vector<uint8_t>& send_key,
                               const std::vector<uint8_t>& recv_key) = 0;
  
  /// 需要发送数据 (作为DTLS客户端/服务器)
  virtual void OnDtlsOutgoingData(const uint8_t* data, size_t size) = 0;
};

/// DTLS接口
class IDtlsTransport {
 public:
  virtual ~IDtlsTransport() = default;
  
  /// 初始化
  virtual void Initialize(const DtlsConfig& config) = 0;
  
  /// 设置回调
  virtual void SetCallback(IDtlsCallback* callback) = 0;
  
  /// 设置ICE传输 (用于DTLS over UDP)
  virtual void SetIceTransport(IIceTransport* ice) = 0;
  
  /// 设置远程DTLS参数 (用于处理Incoming DTLS)
  virtual void SetRemoteFingerprint(const DtlsFingerprint& fingerprint) = 0;
  
  /// 开始握手
  virtual void StartHandshake() = 0;
  
  /// 处理传入数据
  virtual void ProcessIncomingData(const uint8_t* data, size_t size) = 0;
  
  /// 获取本地证书指纹
  virtual DtlsFingerprint GetLocalFingerprint() const = 0;
  
  /// 获取状态
  virtual DtlsState GetState() const = 0;
  
  /// 关闭
  virtual void Close() = 0;
};

}  // namespace minirtc
```

### 8.3 SDP 接口设计

SDP (Session Description Protocol) 用于会话描述和协商。

```cpp
namespace minirtc {

/// SDP媒体类型
enum class SdpMediaType {
  kAudio,
  kVideo,
  kApplication,
  kMessage,
};

/// SDP传输类型
enum class SdpTransportType {
  kUdp,
  kRtpSavp,     // RTP/SAVP (Secure RTP)
  kRtpSavpf,    // RTP/SAVPF (Secure RTP with FEC)
};

/// SDP会话描述
class SdpDescription {
 public:
  virtual ~SdpDescription() = default;
  
  /// 获取版本
  virtual uint32_t GetVersion() const = 0;
  
  /// 获取会话ID
  virtual std::string GetSessionId() const = 0;
  
  /// 获取会话名称
  virtual std::string GetSessionName() const = 0;
  
  /// 获取媒体描述数量
  virtual size_t GetMediaCount() const = 0;
  
  /// 添加媒体描述
  virtual void AddMedia(std::shared_ptr<SdpMediaDescription> media) = 0;
  
  /// 获取媒体描述
  virtual std::shared_ptr<SdpMediaDescription> GetMedia(size_t index) const = 0;
  
  /// 序列化为字符串
  virtual std::string ToString() const = 0;
  
  /// 从字符串解析
  virtual bool FromString(const std::string& sdp) = 0;
};

/// SDP媒体描述
class SdpMediaDescription {
 public:
  virtual ~SdpMediaDescription() = default;
  
  /// 获取媒体类型
  virtual SdpMediaType GetType() const = 0;
  
  /// 获取端口
  virtual uint16_t GetPort() const = 0;
  
  /// 获取传输协议
  virtual SdpTransportType GetTransport() const = 0;
  
  /// 获取载荷类型列表
  virtual std::vector<int> GetPayloadTypes() const = 0;
  
  /// 添加载荷类型
  virtual void AddPayloadType(int pt) = 0;
  
  /// 获取ICE候选列表
  virtual std::vector<IceCandidate> GetCandidates() const = 0;
  
  /// 添加ICE候选
  virtual void AddCandidate(const IceCandidate& candidate) = 0;
  
  /// 获取DTLS指纹
  virtual std::optional<DtlsFingerprint> GetFingerprint() const = 0;
  
  /// 设置DTLS指纹
  virtual void SetFingerprint(const DtlsFingerprint& fp) = 0;
  
  /// 获取端点配置 (e.g., "AVP")
  virtual std::string GetEndpoint() const = 0;
};

/// SDP解析/生成器接口
class ISdpParser {
 public:
  virtual ~ISdpParser() = default;
  
  /// 解析SDP字符串
  virtual std::shared_ptr<SdpDescription> Parse(const std::string& sdp) = 0;
  
  /// 生成SDP字符串
  virtual std::string Generate(const SdpDescription& desc) = 0;
  
  /// 验证SDP有效性
  virtual bool Validate(const SdpDescription& desc) = 0;
};

}  // namespace minirtc
```

---

## 9. 模块依赖关系

| 源模块 | 目标模块 | 依赖关系 |
|--------|----------|----------|
| ITransport | RtpPacket | 强依赖 |
| RTPTransport | ITransport | 实现 |
| RTPTransport | NetworkInterface | 强依赖 |
| RTPTransport | RTCPModule | 强依赖 |
| RTCPModule | RTPTransport | 强依赖 |
| RTCPModule | RtcpPacket | 强依赖 |
| SRTPTransport | RTPTransport | 实现 |
| SRTPTransport | IDtlsTransport | 预留 |
| NetworkInterface | NetworkAddress | 强依赖 |
| IIceTransport | NetworkInterface | 强依赖 |
| IIceTransport | IDtlsTransport | 强依赖 |
| IDtlsTransport | IIceTransport | 强依赖 |
| ISdpParser | SdpDescription | 强依赖 |
| ISdpParser | IceCandidate | 强依赖 |
| ISdpParser | DtlsFingerprint | 强依赖 |

---

## 10. 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v3.0 | 2026-03-10 | 传输层接口设计初稿 |
