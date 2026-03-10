/**
 * @file nack_module.h
 * @brief NACK module for packet loss detection and retransmission request
 */

#ifndef MINIRTC_NACK_MODULE_H
#define MINIRTC_NACK_MODULE_H

#include <cstdint>
#include <memory>
#include <vector>
#include <map>
#include <functional>

#include "minirtc/packet_cache.h"

namespace minirtc {

// ============================================================================
// Configuration
// ============================================================================

/// NACK working mode
enum class NackMode {
  kNone,       // NACK completely disabled
  kRtxOnly,    // Only use RTX retransmission
  kRtcpOnly,   // Only use RTCP NACK feedback
  kAdaptive    // Adaptive mode based on network conditions
};

/// NACK configuration
struct NackConfig {
  /// Enable NACK functionality
  bool enable_nack = true;
  
  /// Enable retransmission
  bool enable_rtx = true;
  
  /// NACK working mode
  NackMode mode = NackMode::kAdaptive;
  
  /// Maximum number of retransmissions
  int max_retransmissions = 3;
  
  /// RTT estimate in milliseconds
  int rtt_estimate_ms = 100;
  
  /// NACK request timeout in milliseconds
  int nack_timeout_ms = 200;
  
  /// Maximum NACK list size
  int max_nack_list_size = 250;
  
  /// NACK batch interval in milliseconds
  int nack_batch_interval_ms = 5;
  
  /// Minimum sequence gap to trigger NACK
  int min_trigger_sequence_gap = 1;
  
  /// Packet loss threshold to trigger NACK
  float packet_loss_threshold = 0.1f;
  
  /// Enable NACK for audio
  bool nack_audio = true;
  
  /// Enable NACK for video
  bool nack_video = true;
  
  /// Enable NACK trace logging
  bool enable_nack_trace = false;
};

// ============================================================================
// Status and Statistics
// ============================================================================

/// NACK status for a single packet
struct NackStatus {
  uint16_t sequence_number;
  int64_t send_time_ms;
  int64_t last_send_time_ms;
  uint8_t retries;
  bool at_risk;
  
  NackStatus()
      : sequence_number(0),
        send_time_ms(0),
        last_send_time_ms(0),
        retries(0),
        at_risk(true) {}
};

/// NACK statistics
struct NackStatistics {
  uint64_t nack_requests_sent;
  uint64_t nack_requests_received;
  uint64_t rtx_packets_sent;
  uint64_t rtx_packets_received;
  uint64_t rtx_success_count;
  uint64_t rtx_timeout_count;
  uint32_t current_nack_list_size;
  int64_t average_rtt_ms;
  
  NackStatistics()
      : nack_requests_sent(0),
        nack_requests_received(0),
        rtx_packets_sent(0),
        rtx_packets_received(0),
        rtx_success_count(0),
        rtx_timeout_count(0),
        current_nack_list_size(0),
        average_rtt_ms(0) {}
};

// ============================================================================
// NACK Module Interface
// ============================================================================

/// Callback types
using OnNackRequestCallback = std::function<void(const std::vector<uint16_t>& seq_nums)>;
using OnRtxPacketCallback = std::function<void(std::shared_ptr<RtpPacket> packet)>;

/// NACK module interface
class INackModule {
 public:
  virtual ~INackModule() = default;

  // ========================================================================
  // Lifecycle
  // ========================================================================

  /// Initialize NACK module
  /// @param config NACK configuration
  /// @return true if initialized successfully
  virtual bool Initialize(const NackConfig& config) = 0;

  /// Start NACK module
  virtual void Start() = 0;

  /// Stop NACK module
  virtual void Stop() = 0;

  /// Reset NACK module state
  virtual void Reset() = 0;

  // ========================================================================
  // RTP Packet Processing
  // ========================================================================

  /// Handle received RTP packet
  /// @param packet RTP packet
  virtual void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) = 0;

  /// Handle received RTX packet
  /// @param packet RTX packet
  virtual void OnRtxPacketReceived(std::shared_ptr<RtpPacket> packet) = 0;

  // ========================================================================
  // NACK List Management
  // ========================================================================

  /// Get list of packets that need NACK
  /// @param current_time_ms Current timestamp
  /// @return List of sequence numbers
  virtual std::vector<uint16_t> GetNackList(int64_t current_time_ms) = 0;

  /// Remove sequence number from NACK list
  /// @param seq_num Sequence number
  virtual void RemoveFromNackList(uint16_t seq_num) = 0;

  /// Check if sequence number is in NACK list
  /// @param seq_num Sequence number
  /// @return true if in list
  virtual bool IsInNackList(uint16_t seq_num) const = 0;

  // ========================================================================
  // RTX Processing
  // ========================================================================

  /// Handle RTX response
  /// @param seq_num Original sequence number
  /// @return true if recovered successfully
  virtual bool HandleRtxResponse(uint16_t seq_num) = 0;

  // ========================================================================
  // Configuration Management
  // ========================================================================

  /// Set NACK configuration
  /// @param config NACK configuration
  virtual void SetConfig(const NackConfig& config) = 0;

  /// Get current NACK configuration
  /// @return NACK configuration
  virtual NackConfig GetConfig() const = 0;

  // ========================================================================
  // Statistics
  // ========================================================================

  /// Get NACK statistics
  /// @return Statistics
  virtual NackStatistics GetStatistics() const = 0;

  /// Reset statistics
  virtual void ResetStatistics() = 0;

  // ========================================================================
  // Callbacks
  // ========================================================================

  /// Set NACK request callback
  /// @param callback Callback function
  virtual void SetOnNackRequestCallback(OnNackRequestCallback callback) = 0;

  /// Set RTX packet callback
  /// @param callback Callback function
  virtual void SetOnRtxPacketCallback(OnRtxPacketCallback callback) = 0;

  // ========================================================================
  // State Query
  // ========================================================================

  /// Check if NACK module is enabled
  /// @return true if enabled
  virtual bool IsEnabled() const = 0;

  /// Get current RTT estimate
  /// @return RTT estimate in milliseconds
  virtual int GetRttEstimate() const = 0;
};

// ============================================================================
// NACK Module Implementation
// ============================================================================

/// NACK module implementation
class NackModule : public INackModule {
 public:
  /// Constructor
  NackModule();
  
  /// Destructor
  ~NackModule() override = default;

  // ========================================================================
  // Lifecycle
  // ========================================================================

  /// Initialize NACK module
  bool Initialize(const NackConfig& config) override;

  /// Start NACK module
  void Start() override;

  /// Stop NACK module
  void Stop() override;

  /// Reset NACK module state
  void Reset() override;

  // ========================================================================
  // RTP Packet Processing
  // ========================================================================

  /// Handle received RTP packet
  void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) override;

  /// Handle received RTX packet
  void OnRtxPacketReceived(std::shared_ptr<RtpPacket> packet) override;

  // ========================================================================
  // NACK List Management
  // ========================================================================

  /// Get list of packets that need NACK
  std::vector<uint16_t> GetNackList(int64_t current_time_ms) override;

  /// Remove sequence number from NACK list
  void RemoveFromNackList(uint16_t seq_num) override;

  /// Check if sequence number is in NACK list
  bool IsInNackList(uint16_t seq_num) const override;

  // ========================================================================
  // RTX Processing
  // ========================================================================

  /// Handle RTX response
  bool HandleRtxResponse(uint16_t seq_num) override;

  // ========================================================================
  // Configuration Management
  // ========================================================================

  /// Set NACK configuration
  void SetConfig(const NackConfig& config) override;

  /// Get current NACK configuration
  NackConfig GetConfig() const override;

  // ========================================================================
  // Statistics
  // ========================================================================

  /// Get NACK statistics
  NackStatistics GetStatistics() const override;

  /// Reset statistics
  void ResetStatistics() override;

  // ========================================================================
  // Callbacks
  // ========================================================================

  /// Set NACK request callback
  void SetOnNackRequestCallback(OnNackRequestCallback callback) override;

  /// Set RTX packet callback
  void SetOnRtxPacketCallback(OnRtxPacketCallback callback) override;

  // ========================================================================
  // State Query
  // ========================================================================

  /// Check if NACK module is enabled
  bool IsEnabled() const override;

  /// Get current RTT estimate
  int GetRttEstimate() const override;

 private:
  /// Add packet to NACK list
  void AddToNackList(uint16_t seq_num, int64_t current_time_ms);
  
  /// Update NACK status
  void UpdateNackStatus(uint16_t seq_num, int64_t current_time_ms);
  
  /// Check if NACK should be sent
  bool ShouldSendNack(const NackStatus& status, int64_t current_time_ms) const;

  /// Configuration
  NackConfig config_;
  
  /// Packet cache
  std::shared_ptr<PacketCache> packet_cache_;
  
  /// NACK list: sequence_number -> NackStatus
  std::map<uint16_t, NackStatus> nack_list_;
  
  /// Statistics
  NackStatistics stats_;
  
  /// Callbacks
  OnNackRequestCallback on_nack_request_;
  OnRtxPacketCallback on_rtx_packet_;
  
  /// Internal state
  bool initialized_;
  bool running_;
  int64_t last_nack_send_time_ms_;
};

// ============================================================================
// Factory
// ============================================================================

/// NACK module factory
class NackModuleFactory {
 public:
  /// Create NACK module
  static std::unique_ptr<INackModule> Create();
};

}  // namespace minirtc

#endif  // MINIRTC_NACK_MODULE_H
