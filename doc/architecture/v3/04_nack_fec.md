# MiniRTC NACK/FEC 模块接口设计文档 v3.0

**版本**: 3.0
**日期**: 2026-03-10
**状态**: 架构设计评审
**前置文档**: MiniRTC 架构设计文档 v2.0

---

## 1. 概述

本文档详细描述MiniRTC v3.0中NACK（Negative Acknowledgment）和FEC（Forward Error Correction）模块的接口设计。这两个模块共同构成抗丢包子系统，在实时通信场景中保障媒体传输的可靠性。

### 1.1 设计目标

- **低延迟**: NACK重传机制需在检测到丢包后尽快发起请求
- **高可靠**: FEC前向纠错可在一定丢包范围内完全恢复丢失数据
- **可配置**: 支持不同网络环境下的灵活配置
- **可扩展**: 预留算法扩展接口，支持多种FEC策略

---

## 2. PacketCache 设计

PacketCache是NACK和FEC模块的共享组件，负责缓存已接收的RTP包，为丢包检测和恢复提供数据基础。

### 2.1 数据结构设计

```cpp
namespace minirtc {

/**
 * @brief RTP包封装类
 */
class RtpPacket {
 public:
  RtpPacket() = default;
  ~RtpPacket() = default;
  
  // 包属性访问
  uint16_t sequence_number() const { return header_.sequence_number; }
  uint32_t timestamp() const { return header_.timestamp; }
  uint8_t payload_type() const { return header_.payload_type; }
  uint32_t ssrc() const { return header_.ssrc; }
  
  // 数据访问
  const uint8_t* data() const { return data_.data(); }
  size_t data_size() const { return data_.size(); }
  size_t payload_size() const { return data_.size() - kRtpHeaderMinSize; }
  
  // 包状态
  bool is_recovered() const { return flags_.recovered; }
  void set_recovered(bool recovered) { flags_.recovered = recovered; }
  
  // 克隆包（用于FEC恢复）
  std::shared_ptr<RtpPacket> Clone() const;
  
 private:
  struct RtpHeader {
    uint16_t sequence_number;
    uint32_t timestamp;
    uint8_t payload_type;
    uint32_t ssrc;
    uint8_t csrc_count;
    bool marker;
    uint8_t extension;
  } header_;
  
  struct {
    bool recovered : 1;    // 是否为恢复包
    bool fec_protected : 1; // 是否已FEC保护
  } flags_;
  
  std::vector<uint8_t> data_;
};

/**
 * @brief 包缓存项
 */
struct PacketCacheItem {
  std::shared_ptr<RtpPacket> packet;
  int64_t received_time_ms;    // 接收时间戳
  bool is_retransmission;       // 是否为重传包
  uint8_t ref_count;            // 引用计数（FEC使用）
};

/**
 * @brief 包缓存配置
 */
struct PacketCacheConfig {
  size_t max_cache_size = 512;        // 最大缓存包数量
  int64_t max_age_ms = 5000;           // 包最大存活时间(ms)
  bool enable_jitter_estimation = true; // 启用抖动估计
};

}  // namespace minirtc
```

### 2.2 PacketCache 核心接口

```cpp
namespace minirtc {

/**
 * @brief PacketCache 回调函数定义
 */
using OnPacketLostCallback = std::function<void(uint16_t seq_num)>;
using OnPacketRecoveredCallback = std::function<void(std::shared_ptr<RtpPacket> packet)>;

/**
 * @brief 包缓存管理器
 * 
 * 提供RTP包的缓存、查询、丢包检测和恢复功能
 */
class PacketCache {
 public:
  explicit PacketCache(const PacketCacheConfig& config);
  ~PacketCache() = default;
  
  // === 基础操作 ===
  
  /**
   * @brief 插入RTP包
   * @param packet 要插入的包
   * @return 插入成功返回true
   */
  bool InsertPacket(std::shared_ptr<RtpPacket> packet);
  
  /**
   * @brief 获取指定序列号的包
   * @param seq_num RTP序列号
   * @return 返回包指针，如果不存在返回nullptr
   */
  std::shared_ptr<RtpPacket> GetPacket(uint16_t seq_num);
  
  /**
   * @brief 检查指定序列号的包是否存在
   * @param seq_num RTP序列号
   * @return 存在返回true
   */
  bool HasPacket(uint16_t seq_num) const;
  
  /**
   * @brief 移除指定序列号的包
   * @param seq_num RTP序列号
   * @return 移除成功返回true
   */
  bool RemovePacket(uint16_t seq_num);
  
  // === 丢包检测 ===
  
  /**
   * @brief 检测丢包并触发回调
   * @param seq_num 新的RTP序列号
   * @param current_time_ms 当前时间戳
   * @return 丢包列表
   */
  std::vector<uint16_t> OnPacketArrived(uint16_t seq_num, int64_t current_time_ms);
  
  // === 批量操作 ===
  
  /**
   * @brief 获取指定范围内的包
   * @param start_seq 起始序列号
   * @param end_seq 结束序列号
   * @return 包列表（按序列号排序）
   */
  std::vector<std::shared_ptr<RtpPacket>> GetPacketsInRange(
      uint16_t start_seq, uint16_t end_seq) const;
  
  /**
   * @brief 获取所有缓存的包
   * @return 所有包列表
   */
  std::vector<std::shared_ptr<RtpPacket>> GetAllPackets() const;
  
  // === 状态管理 ===
  
  /**
   * @brief 获取当前缓存大小
   * @return 缓存的包数量
   */
  size_t size() const { return cache_.size(); }
  
  /**
   * @brief 清空缓存
   */
  void Clear();
  
  /**
   * @brief 清理过期包
   * @param current_time_ms 当前时间戳
   * @return 清理的包数量
   */
  size_t CleanupExpiredPackets(int64_t current_time_ms);
  
  // === 统计 ===
  
  /**
   * @brief 获取丢包统计信息
   */
  struct Statistics {
    uint64_t total_packets_received;
    uint64_t total_packets_lost;
    uint64_t total_packets_recovered;
    uint32_t current_seq;
    int64_t last_packet_time_ms;
  };
  
  Statistics GetStatistics() const;
  
  // === 回调设置 ===
  
  void SetOnPacketLostCallback(OnPacketLostCallback callback);
  void SetOnPacketRecoveredCallback(OnPacketRecoveredCallback callback);
  
  // === 配置更新 ===
  
  void SetConfig(const PacketCacheConfig& config);

 private:
  bool IsOldPacket(uint16_t seq_num, int64_t current_time_ms) const;
  uint16_t GetExpectedSequenceNumber() const;
  
  PacketCacheConfig config_;
  
  // 缓存存储：sequence_number -> PacketCacheItem
  std::map<uint16_t, PacketCacheItem> cache_;
  
  // 统计信息
  Statistics stats_;
  
  // 回调
  OnPacketLostCallback on_packet_lost_;
  OnPacketRecoveredCallback on_packet_recovered_;
  
  // 内部状态
  uint16_t highest_seq_num_;
  int64_t last_update_time_ms_;
};

}  // namespace minirtc
```

### 2.3 PacketCache 工作流程

```
                    PacketCache 工作流程
                    =====================
                    
    +-----------------+
    |  RTP包到达      |
    |  seq_num = N    |
    +--------+--------+
             |
             v
    +-----------------+
    |  检测序列号      |
    |  N > Exp ?     |
    +--------+--------+
             |
     +-------+-------+
     |               |
     v               v
+---------+    +---------+
| Yes     |    | No      |
| (新包)  |    |(过期/重复)|
+----+----+    +----+----+
     |              |
     v              v
+---------+   +---------+
| 丢包检测 |   | 丢弃包  |
| 返回缺失 |   | 返回    |
| 序列号   |   +---------+
+----+----+
     |
     v
+-----------------+
| 触发丢包回调    |
| 记录丢包统计    |
+--------+--------+
         |
         v
+-----------------+
| 插入缓存        |
| cache_[N]=item |
+--------+--------+
         |
         v
+-----------------+
| 更新最高序列号   |
| highest = N     |
+-----------------+
```

---

## 3. NACK Module 详细接口设计

### 3.1 NACK 配置结构

```cpp
namespace minirtc {

/**
 * @brief NACK工作模式
 */
enum class NackMode {
  kNone,        // 完全禁用NACK
  kRtxOnly,     // 仅使用RTX重传
  kRtcpOnly,    // 仅使用RTCP NACK反馈
  kAdaptive,    // 自适应模式（根据网络状况选择）
};

/**
 * @brief NACK配置
 */
struct NackConfig {
  // 基础开关
  bool enable_nack = true;           // 启用NACK功能
  bool enable_rtx = true;            // 启用重传功能
  
  // NACK模式
  NackMode mode = NackMode::kAdaptive;  // NACK工作模式
  
  // 重传参数
  int max_retransmissions = 3;       // 最大重传次数
  int rtt_estimate_ms = 100;         // RTT估计值(ms)
  int nack_timeout_ms = 200;         // NACK请求超时时间(ms)
  
  // NACK列表管理
  int max_nack_list_size = 250;      // NACK列表最大大小
  int nack_batch_interval_ms = 5;   // NACK批处理间隔(ms)
  
  // 触发阈值
  int min_trigger_sequence_gap = 1;  // 触发NACK的最小序列号间隔
  float packet_loss_threshold = 0.1f; // 触发NACK的丢包率阈值
  
  // 媒体类型
  bool nack_audio = true;            // 对音频启用NACK
  bool nack_video = true;            // 对视频启用NACK
  
  // 调试
  bool enable_nack_trace = false;    // 启用NACK追踪日志
};

/**
 * @brief NACK状态信息
 */
struct NackStatus {
  uint16_t sequence_number;           // RTP序列号
  int64_t send_time_ms;              // 首次发送NACK的时间
  int64_t last_send_time_ms;         // 最近一次发送NACK的时间
  uint8_t retries;                   // 已重试次数
  bool at_risk;                      // 是否处于风险状态
};

/**
 * @brief NACK统计信息
 */
struct NackStatistics {
  uint64_t nack_requests_sent;       // 已发送的NACK请求数
  uint64_t nack_requests_received;   // 接收到的NACK请求数
  uint64_t rtx_packets_sent;        // 发送的重传包数
  uint64_t rtx_packets_received;    // 接收的重传包数
  uint64_t rtx_success_count;        // 重传成功次数
  uint64_t rtx_timeout_count;        // 重传超时次数
  uint32_t current_nack_list_size;  // 当前NACK列表大小
  int64_t average_rtt_ms;            // 平均RTT
};

}  // namespace minirtc
```

### 3.2 NACK 核心接口

```cpp
namespace minirtc {

/**
 * @brief NACK请求回调
 * @param seq_nums 需要重传的序列号列表
 */
using OnNackRequestCallback = std::function<void(const std::vector<uint16_t>& seq_nums)>;

/**
 * @brief RTX包回调
 * @param packet RTX重传包
 */
using OnRtxPacketCallback = std::function<void(std::shared_ptr<RtpPacket> packet)>;

/**
 * @brief NACK模块接口
 * 
 * 负责检测丢包、生成NACK请求、处理RTX重传
 */
class INackModule {
 public:
  virtual ~INackModule() = default;
  
  // === 生命周期 ===
  
  /**
   * @brief 初始化NACK模块
   * @param config NACK配置
   * @return 初始化成功返回true
   */
  virtual bool Initialize(const NackConfig& config) = 0;
  
  /**
   * @brief 启动NACK模块
   */
  virtual void Start() = 0;
  
  /**
   * @brief 停止NACK模块
   */
  virtual void Stop() = 0;
  
  /**
   * @brief 重置NACK模块状态
   */
  virtual void Reset() = 0;
  
  // === RTP包处理 ===
  
  /**
   * @brief 处理接收到的RTP包
   * @param packet RTP包
   */
  virtual void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) = 0;
  
  /**
   * @brief 处理接收到的RTX包
   * @param packet RTX包
   */
  virtual void OnRtxPacketReceived(std::shared_ptr<RtpPacket> packet) = 0;
  
  // === NACK列表管理 ===
  
  /**
   * @brief 获取当前需要NACK的包列表
   * @param current_time_ms 当前时间戳
   * @return 序列号列表
   */
  virtual std::vector<uint16_t> GetNackList(int64_t current_time_ms) = 0;
  
  /**
   * @brief 从NACK列表中移除指定序列号
   * @param seq_num 序列号
   */
  virtual void RemoveFromNackList(uint16_t seq_num) = 0;
  
  /**
   * @brief 检查序列号是否在NACK列表中
   * @param seq_num 序列号
   * @return 是否在列表中
   */
  virtual bool IsInNackList(uint16_t seq_num) const = 0;
  
  // === RTX处理 ===
  
  /**
   * @brief 处理RTX重传响应
   * @param seq_num 原始序列号
   * @return 恢复成功返回true
   */
  virtual bool HandleRtxResponse(uint16_t seq_num) = 0;
  
  // === 配置管理 ===
  
  /**
   * @brief 设置NACK配置
   * @param config NACK配置
   */
  virtual void SetConfig(const NackConfig& config) = 0;
  
  /**
   * @brief 获取当前NACK配置
   * @return NACK配置
   */
  virtual NackConfig GetConfig() const = 0;
  
  // === 统计 ===
  
  /**
   * @brief 获取NACK统计信息
   * @return 统计信息
   */
  virtual NackStatistics GetStatistics() const = 0;
  
  /**
   * @brief 重置统计信息
   */
  virtual void ResetStatistics() = 0;
  
  // === 回调设置 ===
  
  /**
   * @brief 设置NACK请求发送回调
   * @param callback 回调函数
   */
  virtual void SetOnNackRequestCallback(OnNackRequestCallback callback) = 0;
  
  /**
   * @brief 设置RTX包接收回调
   * @param callback 回调函数
   */
  virtual void SetOnRtxPacketCallback(OnRtxPacketCallback callback) = 0;
  
  // === 状态查询 ===
  
  /**
   * @brief 检查NACK模块是否启用
   * @return 是否启用
   */
  virtual bool IsEnabled() const = 0;
  
  /**
   * @brief 获取当前RTT估计值
   * @return RTT估计值(ms)
   */
  virtual int GetRttEstimate() const = 0;
};

/**
 * @brief NACK模块工厂
 */
class NackModuleFactory {
 public:
  static std::unique_ptr<INackModule> Create();
};

}  // namespace minirtc
```

### 3.3 NACK 工作流程

```
                        NACK 工作流程
                        ==============
                        
接收端                                                       发送端
--------------------------------------------------------------------

+-----------------+                                    +-----------------+
|  RTP包到达      |                                    |                 |
| seq_num = N    |                                    |                 |
+-----------------+                                    |                 |
             |                                          |                 |
             v                                          |                 |
+-----------------+                                    |                 |
| PacketCache     |                                    |                 |
| 检测丢包        |----+                             |                 |
+-----------------+      |                             |                 |
             |         +----+----+                      |                 |
             |         |         |                      |                 |
             v         v         v                      |                 |
+-----------+   +-----------+   +-----------+          |                 |
| 无丢包    |   | 丢包检测  |   | 重复包    |          |                 |
| 更新缓存  |   | 生成NACK  |   | 丢弃/处理 |          |                 |
+-----+-----+   +-----+-----+   +-----------+          |                 |
      |            |                                     |                 |
      |            v                                     |                 |
      |     +-----------+                                |                 |
      |     | 加入NACK  |                                |                 |
      |     | 列表      |                                |                 |
      |     +-----+-----+                                |                 |
      |           |                                       |                 |
      |           v                                       |                 |
      |     +-----------+      +---------------------+   |                 |
      |     | 定时器    |----->| 发送NACK请求        |   |                 |
      |     | 触发      |      | (RTCP NACK / RTX)   |   |                 |
      |     +-----------+      +----------+--------+   |                 |
      |                                   |              |                 |
      |                                   |   RTCP       |                 |
      +---------------------------------->|  NACK        |                 |
                                +-----------+              |                 |
                                |                         |                 |
                                v                         |                 |
+-----------------+     +-----------------+               |                 |
|                 |<----| 接收NACK请求    |               |                 |
|                 |     | 查找对应包      |               |                 |
|                 |     | 发送RTX包       |               |                 |
+-----------------+     +--------+--------+               |                 |
                                           |              |                 |
                                           v              |                 |
                                 +-----------------+       |                 |
                                 |  RTX包到达      |       |                 |
                                 | seq_num = N    |       |                 |
                                 +--------+--------+       |                 |
                                          |              |                 |
                                          v              |                 |
                                 +-----------------+       |                 |
                                 | PacketCache.Insert     |                 |
                                 | 从NACK列表移除         |                 |
                                 | 标记为已恢复           |                 |
                                 +--------+--------+       |                 |
                                          |              |                 |
                                          v              |                 |
                                 +-----------------+       |                 |
                                 | 送入JitterBuffer|       |                 |
                                 +-----------------+       |                 |
```

### 3.4 NACK 序列图

```
                           NACK 完整序列图
                           ===============

   发送端                  通道                     接收端
     |                     |                         |
     |    RTP包(seq=100)   |                         |
     |--------------------->|                         |
     |    RTP包(seq=101)   |                         |
     |--------------------->|                         |
     |    RTP包(seq=102)   |                         |
     |--------------------->|                         |
     |                [丢包: seq=103]               |
     |                     |                         |
     |    RTP包(seq=104)   |                         |
     |--------------------->|                         |
     |                     |                         |  检测到seq=103丢失
     |                     |                         |  添加到NACK列表
     |                     |                         |
     |                     |    RTCP NACK (seq=103)  |
     |<--------------------|                         |
     |                     |                         |  NACK超时检查
     |                     |                         |  重发NACK请求
     |                     |    RTCP NACK (seq=103)  |
     |<--------------------|                         |
     |                     |                         |
     |  RTX包(seq=103)    |                         |
     |--------------------->|                         |
     |                     |                         |  RTX包处理
     |                     |                         |  恢复seq=103
     |                     |                         |  从NACK列表移除
     |                     |                         |
     |                     |    RTP包(seq=105)      |
     |--------------------->|                         |
     |                     |                         |
```

---

## 4. FEC Module 详细接口设计

### 4.1 FEC 配置结构

```cpp
namespace minirtc {

/**
 * @brief FEC算法类型
 */
enum class FecAlgorithm {
  kNone,       // 禁用FEC
  kXorFec,     // 简单XOR纠错
  kUlpFec,     // 不等保护FEC (Unequal Level Protection)
  kHybrid,     // 混合模式
};

/**
 * @brief FEC级别定义
 */
enum class FecLevel {
  kLow,        // 低保护：5-10%冗余
  kMedium,     // 中保护：10-20%冗余
  kHigh,       // 高保护：20-30%冗余
  kUltra,      // 超高保护：30-50%冗余
};

/**
 * @brief FEC配置
 */
struct FecConfig {
  // 基础开关
  bool enable_fec = true;             // 启用FEC功能
  FecAlgorithm algorithm = FecAlgorithm::kUlpFec;  // FEC算法
  
  // 冗余配置
  FecLevel fec_level = FecLevel::kMedium;  // FEC保护级别
  int fec_percentage = 15;            // 冗余百分比 (5-50)
  int min_fec_percentage = 5;         // 最小冗余百分比
  int max_fec_percentage = 50;        // 最大冗余百分比
  
  // RTP负载配置
  int media_payload_type = 96;        // 媒体 RTP PT
  int fec_payload_type = 97;          // FEC RTP PT
  
  // 编码参数
  int max_fec_group_size = 48;        // 最大FEC组大小(包数)
  int fec_group_interval_ms = 20;     // FEC组时间间隔(ms)
  
  // 自适应配置
  bool enable_adaptive_fec = true;   // 启用自适应FEC
  float packet_loss_rate_threshold = 0.05f;  // 触发FEC的丢包率阈值
  
  // 媒体类型
  bool fec_audio = false;             // 对音频启用FEC
  bool fec_video = true;              // 对视频启用FEC
  
  // ULP FEC特定配置
  int media_protection_ratio = 70;    // 媒体包保护比例 (0-100)
  
  // 调试
  bool enable_fec_trace = false;     // 启用FEC追踪日志
};

/**
 * @brief FEC组信息
 */
struct FecGroup {
  uint16_t group_id;                  // FEC组ID
  uint16_t start_seq;                 // 起始序列号
  uint16_t end_seq;                   // 结束序列号
  int64_t timestamp;                  // RTP时间戳
  int media_count;                    // 媒体包数量
  int fec_count;                     // FEC包数量
  bool complete;                      // 组是否完整
};

/**
 * @brief FEC统计信息
 */
struct FecStatistics {
  uint64_t fec_packets_sent;          // 发送的FEC包数
  uint64_t fec_packets_received;      // 接收的FEC包数
  uint64_t packets_recovered;        // 恢复的包数
  uint64_t recovery_success_count;    // 成功恢复次数
  uint64_t recovery_failure_count;    // 恢复失败次数
  uint64_t fec_overhead_bytes;       // FEC开销字节数
  float current_fec_percentage;       // 当前FEC冗余率
};

}  // namespace minirtc
```

### 4.2 FEC 核心接口

```cpp
namespace minirtc {

/**
 * @brief FEC编码结果回调
 * @param fec_packets 生成的FEC包列表
 */
using OnFecEncodeCallback = std::function<void(const std::vector<std::shared_ptr<RtpPacket>>& fec_packets)>;

/**
 * @brief FEC恢复结果回调
 * @param recovered_packets 恢复的包列表
 */
using OnFecRecoverCallback = std::function<void(const std::vector<std::shared_ptr<RtpPacket>>& recovered_packets)>;

/**
 * @brief FEC模块接口
 * 
 * 负责FEC编码（发送端）和FEC解码（接收端）
 */
class IFecModule {
 public:
  virtual ~IFecModule() = default;
  
  // === 生命周期 ===
  
  /**
   * @brief 初始化FEC模块
   * @param config FEC配置
   * @return 初始化成功返回true
   */
  virtual bool Initialize(const FecConfig& config) = 0;
  
  /**
   * @brief 启动FEC模块
   */
  virtual void Start() = 0;
  
  /**
   * @brief 停止FEC模块
   */
  virtual void Stop() = 0;
  
  /**
   * @brief 重置FEC模块状态
   */
  virtual void Reset() = 0;
  
  // === 发送端：FEC编码 ===
  
  /**
   * @brief 添加媒体包到FEC编码器
   * @param packet 媒体RTP包
   * @return 添加成功返回true
   */
  virtual bool AddMediaPacket(std::shared_ptr<RtpPacket> packet) = 0;
  
  /**
   * @brief 触发FEC编码并生成冗余包
   * @return FEC包列表
   */
  virtual std::vector<std::shared_ptr<RtpPacket>> EncodeFec() = 0;
  
  /**
   * @brief 获取待发送的FEC包
   * @return FEC包列表
   */
  virtual std::vector<std::shared_ptr<RtpPacket>> GetPendingFecPackets() = 0;
  
  /**
   * @brief 清除待发送的FEC包队列
   */
  virtual void ClearPendingFecPackets() = 0;
  
  // === 接收端：FEC解码 ===
  
  /**
   * @brief 处理接收到的RTP包
   * @param packet RTP包
   * @return 恢复的包列表（如果有）
   */
  virtual std::vector<std::shared_ptr<RtpPacket>> OnRtpPacketReceived(
      std::shared_ptr<RtpPacket> packet) = 0;
  
  /**
   * @brief 处理接收到的FEC包
   * @param packet FEC包
   */
  virtual void OnFecPacketReceived(std::shared_ptr<RtpPacket> packet) = 0;
  
  /**
   * @brief 尝试恢复丢失的包
   * @param missing_seq 缺失的序列号
   * @return 恢复的包（如果有）
   */
  virtual std::shared_ptr<RtpPacket> TryRecoverPacket(uint16_t missing_seq) = 0;
  
  /**
   * @brief 手动触发FEC解码尝试
   * @return 恢复的包列表
   */
  virtual std::vector<std::shared_ptr<RtpPacket>> TryDecodeAll() = 0;
  
  // === 配置管理 ===
  
  /**
   * @brief 设置FEC配置
   * @param config FEC配置
   */
  virtual void SetConfig(const FecConfig& config) = 0;
  
  /**
   * @brief 获取当前FEC配置
   * @return FEC配置
   */
  virtual FecConfig GetConfig() const = 0;
  
  /**
   * @brief 更新FEC级别（运行时）
   * @param level 新的FEC级别
   */
  virtual void UpdateFecLevel(FecLevel level) = 0;
  
  // === 统计 ===
  
  /**
   * @brief 获取FEC统计信息
   * @return 统计信息
   */
  virtual FecStatistics GetStatistics() const = 0;
  
  /**
   * @brief 重置统计信息
   */
  virtual void ResetStatistics() = 0;
  
  // === 回调设置 ===
  
  /**
   * @brief 设置FEC编码完成回调
   * @param callback 回调函数
   */
  virtual void SetOnFecEncodeCallback(OnFecEncodeCallback callback) = 0;
  
  /**
   * @brief 设置FEC恢复完成回调
   * @param callback 回调函数
   */
  virtual void SetOnFecRecoverCallback(OnFecRecoverCallback callback) = 0;
  
  // === 状态查询 ===
  
  /**
   * @brief 检查FEC模块是否启用
   * @return 是否启用
   */
  virtual bool IsEnabled() const = 0;
  
  /**
   * @brief 获取当前FEC组信息
   * @return FEC组列表
   */
  virtual std::vector<FecGroup> GetFecGroups() const = 0;
};

/**
 * @brief FEC模块工厂
 */
class FecModuleFactory {
 public:
  /**
   * @brief 创建指定算法的FEC模块
   * @param algorithm FEC算法
   * @return FEC模块指针
   */
  static std::unique_ptr<IFecModule> Create(FecAlgorithm algorithm);
  
  /**
   * @brief 创建UlpFec模块（默认）
   */
  static std::unique_ptr<IFecModule> CreateUlpFec();
  
  /**
   * @brief 创建XorFec模块
   */
  static std::unique_ptr<IFecModule> CreateXorFec();
};

}  // namespace minirtc
```

---

## 5. FEC 算法设计

### 5.1 XOR FEC 算法

XOR FEC是最简单的FEC算法，通过对组内所有包进行XOR运算生成冗余包。

```cpp
namespace minirtc {

/**
 * @brief XOR FEC 编码器
 * 
 * 算法原理：
 * - 将N个媒体包分为一组
 * - 对每个字节位置进行XOR运算，生成1个FEC包
 * - 任意丢失1个包时，可通过FEC包恢复
 * 
 * 开销：1/N * 100%
 */
class XorFecEncoder {
 public:
  XorFecEncoder() = default;
  ~XorFecEncoder() = default;
  
  /**
   * @brief 添加媒体包到编码组
   * @param packet 媒体包
   * @return 添加成功返回true
   */
  bool AddPacket(std::shared_ptr<RtpPacket> packet);
  
  /**
   * @brief 执行XOR编码
   * @return 生成的FEC包
   */
  std::shared_ptr<RtpPacket> Encode();
  
  /**
   * @brief 清空编码组
   */
  void Clear();
  
  /**
   * @brief 获取当前组大小
   * @return 组内包数量
   */
  size_t group_size() const { return packets_.size(); }
  
  /**
   * @brief 检查是否可以编码
   * @return 组大小>0返回true
   */
  bool can_encode() const { return !packets_.empty(); }

 private:
  std::vector<std::shared_ptr<RtpPacket>> packets_;
};

/**
 * @brief XOR FEC 解码器
 */
class XorFecDecoder {
 public:
  XorFecDecoder() = default;
  ~XorFecDecoder() = default;
  
  /**
   * @brief 添加FEC包到解码器
   * @param fec_packet FEC包
   */
  void AddFecPacket(std::shared_ptr<RtpPacket> fec_packet);
  
  /**
   * @brief 添加媒体包到解码器
   * @param media_packet 媒体包
   */
  void AddMediaPacket(std::shared_ptr<RtpPacket> media_packet);
  
  /**
   * @brief 尝试恢复指定序列号的包
   * @param seq_num 要恢复的序列号
   * @return 恢复的包（如果可以恢复）
   */
  std::shared_ptr<RtpPacket> TryRecover(uint16_t seq_num);
  
  /**
   * @brief 尝试恢复所有可恢复的包
   * @return 恢复的包列表
   */
  std::vector<std::shared_ptr<RtpPacket>> RecoverAll();
  
  /**
   * @brief 清空解码器状态
   */
  void Clear();

 private:
  std::vector<std::shared_ptr<RtpPacket>> media_packets_;
  std::shared_ptr<RtpPacket> fec_packet_;
};

}  // namespace minirtc
```

### 5.2 ULP FEC 算法

ULP FEC (Unequal Level Protection FEC) 提供不等保护，对不同重要性的数据提供不同级别的保护。

```cpp
namespace minirtc {

/**
 * @brief ULP FEC 保护级别
 */
struct UlpFecLevel {
  int16_t priority;           // 优先级 (-1=不保护, 0=低, 1=中, 2=高)
  uint8_t protection_count;   // 保护次数
};

/**
 * @brief ULP FEC RTP头部扩展
 */
struct UlpFecExtension {
  uint16_t fec_group_id;       // FEC组ID
  uint8_t fec_group_size;      // FEC组大小
  uint8_t protection_count;    // 保护计数
  int16_t priority;            // 优先级
  std::vector<uint16_t> seq_nums;  // 被保护的序列号列表
};

/**
 * @brief ULP FEC 编码器
 * 
 * 算法原理：
 * - 将媒体包按优先级分类
 * - 对不同优先级使用不同的保护级别
 * - 高优先级数据生成多个FEC包
 * - 低优先级数据生成较少FEC包或仅使用XOR保护
 * 
 * 示例（媒体保护比例70%）：
 * - 高优先级包：生成Level 2 FEC包（2次XOR）
 * - 中优先级包：生成Level 1 FEC包（1次XOR）
 * - 低优先级包：使用Level 0 XOR保护
 */
class UlpFecEncoder {
 public:
  UlpFecEncoder();
  ~UlpFecEncoder() = default;
  
  /**
   * @brief 配置保护参数
   * @param media_protection_ratio 媒体保护比例(0-100)
   */
  void SetProtectionRatio(int media_protection_ratio);
  
  /**
   * @brief 添加媒体包
   * @param packet 媒体包
   * @param priority 优先级
   * @return 添加成功返回true
   */
  bool AddPacket(std::shared_ptr<RtpPacket> packet, int16_t priority = 0);
  
  /**
   * @brief 执行ULP FEC编码
   * @return 生成的FEC包列表
   */
  std::vector<std::shared_ptr<RtpPacket>> Encode();
  
  /**
   * @brief 获取FEC头部扩展信息
   * @return FEC扩展信息
   */
  const UlpFecExtension& GetFecExtension() const { return fec_extension_; }
  
  /**
   * @brief 清空编码器
   */
  void Clear();
  
  /**
   * @brief 获取当前组大小
   */
  size_t group_size() const { return packets_.size(); }

 private:
  void GenerateProtectionLevels();
  std::shared_ptr<RtpPacket> GenerateFecForLevel(
      const std::vector<std::shared_ptr<RtpPacket>>& packets, int16_t level);
  
  int protection_ratio_;
  std::vector<std::shared_ptr<RtpPacket>> packets_;
  std::map<int16_t, std::vector<std::shared_ptr<RtpPacket>>> priority_groups_;
  UlpFecExtension fec_extension_;
};

/**
 * @brief ULP FEC 解码器
 */
class UlpFecDecoder {
 public:
  UlpFecDecoder();
  ~UlpFecDecoder() = default;
  
  /**
   * @brief 添加FEC包到解码器
   * @param fec_packet FEC包
   */
  void AddFecPacket(std::shared_ptr<RtpPacket> fec_packet);
  
  /**
   * @brief 添加媒体包到解码器
   * @param media_packet 媒体包
   */
  void AddMediaPacket(std::shared_ptr<RtpPacket> media_packet);
  
  /**
   * @brief 尝试恢复指定序列号的包
   * @param seq_num 要恢复的序列号
   * @return 恢复的包（如果可以恢复）
   */
  std::shared_ptr<RtpPacket> TryRecover(uint16_t seq_num);
  
  /**
   * @brief 尝试恢复所有可恢复的包
   * @return 恢复的包列表
  std::vector<std::shared_ptr<RtpPacket>> RecoverAll();
  
  /**
   * @brief 清空解码器
   */
  void Clear();

 private:
  bool CanRecover(uint16_t seq_num) const;
  std::shared_ptr<RtpPacket> RecoverWithLevel(
      uint16_t seq_num, int16_t level);
  
  std::map<int16_t, std::vector<std::shared_ptr<RtpPacket>>> fec_packets_by_level_;
  std::vector<std::shared_ptr<RtpPacket>> media_packets_;
};

}  // namespace minirtc
```

### 5.3 FEC 工作流程

```
                        FEC 工作流程
                        =============

发送端（编码）                                     接收端（解码）
--------                                      --------

+-----------------+                           +-----------------+
| 媒体帧到达      |                           |                 |
| 分包为RTP包    |                           |                 |
+--------+--------+                          |                 |
         |                                   |                 |
         v                                   |                 |
+-----------------+                          |                 |
| 添加到FEC组     |                          |                 |
| (按时间/数量)  |                          |                 |
+--------+--------+                          |                 |
         |                                   |                 |
         v                                   |                 |
+-----------------+                          |                 |
| 组满/定时触发   |                          |                 |
| FEC编码         |                          |                 |
+--------+--------+                          |                 |
         |                                   |                 |
         v                                   |                 |
+-----------------+     +-----------------+  |                 |
| 生成FEC包       |---->| 混合发送        |  |                 |
| (RTP负载类型97) |     | (Media + FEC)  |  |                 |
+-----------------+     +-----------------+  |                 |
                                           |                 |
                                           |                 |
                                           |                 |
                                           |    +-----------------+
                                           |    | RTP包到达       |
                                           |    | (Media or FEC)  |
                                           |    +--------+--------+
                                           |             |
                                           |             v
                                           |    +-----------------+
                                           |    | 判断包类型      |
                                           |    +--------+--------+
                                           |             |
                                           |     +-------+-------+
                                           |     |               |
                                           |     v               v
                                           | +---------+   +---------+
                                           | | 媒体包  |   | FEC包   |
                                           | | 解码   |   | 缓存   |
                                           | +----+---+   +----+----+
                                           |      |            |
                                           |      v            v
                                           | +-----------------+   +-----------------+
                                           | | 检测丢包       |   | 关联FEC组      |
                                           | | 尝试FEC恢复    |◄─►| 缓存FEC包     |
                                           | +--------+-------+   +--------+--------+
                                           |          |                    |
                                           |          +--------+-----------+
                                           |                   |
                                           |                   v
                                           |          +-----------------+
                                           |          | FEC解码成功     |
                                           |          | 恢复丢失包      |
                                           |          +--------+--------+
                                           |                   |
                                           |                   v
                                           |          +-----------------+
                                           |          | 送入JitterBuffer|
                                           |          +-----------------+
                                           +<-RTP包--+                  

```

---

## 6. 模块集成

### 6.1 NACK与FEC协作

```
                    NACK + FEC 协作架构
                    ==================

         ┌─────────────────────────────────────────┐
         │           抗丢包子系统                   │
         │  ┌───────────────────────────────────┐  │
         │  │         PacketCache              │  │
         │  │    (共享缓存，NACK和FEC共用)     │  │
         │  └───────────────┬─────────────────┘  │
         │                  │                      │
         │      ┌───────────┴───────────┐         │
         │      │                       │         │
         │      ▼                       ▼         │
         │  ┌───────────┐         ┌───────────┐   │
         │  │   NACK   │         │   FEC    │   │
         │  │  Module  │         │  Module  │   │
         │  └─────┬─────┘         └─────┬─────┘   │
         │        │                     │          │
         │        │ 丢包检测           │ FEC恢复  │
         │        │ NACK请求           │ 恢复包   │
         │        │ RTX重传            │ 送入缓冲 │
         │        ▼                     ▼          │
         │  ┌───────────────────────────────────┐  │
         │  │         JitterBuffer              │  │
         │  └───────────────────────────────────┘  │
         └─────────────────────────────────────────┘
```

### 6.2 丢包恢复策略

1. **FEC优先**: 丢包时首先尝试FEC恢复（主动，无需反馈）
2. **NACK补充**: FEC无法恢复时，通过NACK请求重传（被动）
3. **自适应选择**: 根据网络状况动态调整FEC冗余度和NACK策略

---

## 7. 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v3.0 | 2026-03-10 | 初始版本：NACK/FEC模块详细接口设计 |

---

*本文档为MiniRTC v3.0架构设计的一部分，需要与v2.0设计文档配合阅读。*
