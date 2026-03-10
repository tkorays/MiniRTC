/**
 * @file fec_module.h
 * @brief FEC module for forward error correction
 */

#ifndef MINIRTC_FEC_MODULE_H
#define MINIRTC_FEC_MODULE_H

#include <cstdint>
#include <memory>
#include <vector>
#include <map>
#include <functional>

#include "minirtc/transport/rtp_packet.h"

namespace minirtc {

// ============================================================================
// Configuration
// ============================================================================

/// FEC algorithm type
enum class FecAlgorithm {
  kNone,       // FEC disabled
  kXorFec,     // Simple XOR FEC
  kUlpFec,     // Unequal Level Protection FEC
  kHybrid      // Hybrid mode
};

/// FEC protection level
enum class FecLevel {
  kLow,        // Low protection: 5-10% redundancy
  kMedium,     // Medium protection: 10-20% redundancy
  kHigh,       // High protection: 20-30% redundancy
  kUltra       // Ultra protection: 30-50% redundancy
};

/// FEC configuration
struct FecConfig {
  /// Enable FEC functionality
  bool enable_fec = true;
  
  /// FEC algorithm
  FecAlgorithm algorithm = FecAlgorithm::kXorFec;
  
  /// FEC protection level
  FecLevel fec_level = FecLevel::kMedium;
  
  /// FEC redundancy percentage (5-50)
  int fec_percentage = 15;
  
  /// Minimum FEC redundancy percentage
  int min_fec_percentage = 5;
  
  /// Maximum FEC redundancy percentage
  int max_fec_percentage = 50;
  
  /// Media RTP payload type
  int media_payload_type = 96;
  
  /// FEC RTP payload type
  int fec_payload_type = 97;
  
  /// Maximum FEC group size (number of packets)
  int max_fec_group_size = 48;
  
  /// FEC group interval in milliseconds
  int fec_group_interval_ms = 20;
  
  /// Enable adaptive FEC
  bool enable_adaptive_fec = true;
  
  /// Packet loss rate threshold to trigger FEC
  float packet_loss_rate_threshold = 0.05f;
  
  /// Enable FEC for audio
  bool fec_audio = false;
  
  /// Enable FEC for video
  bool fec_video = true;
  
  /// Media protection ratio for ULP FEC (0-100)
  int media_protection_ratio = 70;
  
  /// Enable FEC trace logging
  bool enable_fec_trace = false;
};

// ============================================================================
// Status and Statistics
// ============================================================================

/// FEC group information
struct FecGroup {
  uint16_t group_id;
  uint16_t start_seq;
  uint16_t end_seq;
  int64_t timestamp;
  int media_count;
  int fec_count;
  bool complete;
  
  FecGroup()
      : group_id(0),
        start_seq(0),
        end_seq(0),
        timestamp(0),
        media_count(0),
        fec_count(0),
        complete(false) {}
};

/// FEC statistics
struct FecStatistics {
  uint64_t fec_packets_sent;
  uint64_t fec_packets_received;
  uint64_t packets_recovered;
  uint64_t recovery_success_count;
  uint64_t recovery_failure_count;
  uint64_t fec_overhead_bytes;
  float current_fec_percentage;
  
  FecStatistics()
      : fec_packets_sent(0),
        fec_packets_received(0),
        packets_recovered(0),
        recovery_success_count(0),
        recovery_failure_count(0),
        fec_overhead_bytes(0),
        current_fec_percentage(0.0f) {}
};

// ============================================================================
// Callback Types
// ============================================================================

using OnFecEncodeCallback = std::function<void(const std::vector<std::shared_ptr<RtpPacket>>& fec_packets)>;
using OnFecRecoverCallback = std::function<void(const std::vector<std::shared_ptr<RtpPacket>>& recovered_packets)>;

// ============================================================================
// FEC Module Interface
// ============================================================================

/// FEC module interface
class IFecModule {
 public:
  virtual ~IFecModule() = default;

  // ========================================================================
  // Lifecycle
  // ========================================================================

  /// Initialize FEC module
  /// @param config FEC configuration
  /// @return true if initialized successfully
  virtual bool Initialize(const FecConfig& config) = 0;

  /// Start FEC module
  virtual void Start() = 0;

  /// Stop FEC module
  virtual void Stop() = 0;

  /// Reset FEC module state
  virtual void Reset() = 0;

  // ========================================================================
  // Sender: FEC Encoding
  // ========================================================================

  /// Add media packet to FEC encoder
  /// @param packet Media RTP packet
  /// @return true if added successfully
  virtual bool AddMediaPacket(std::shared_ptr<RtpPacket> packet) = 0;

  /// Trigger FEC encoding and generate redundancy packets
  /// @return List of FEC packets
  virtual std::vector<std::shared_ptr<RtpPacket>> EncodeFec() = 0;

  /// Get pending FEC packets to send
  /// @return List of FEC packets
  virtual std::vector<std::shared_ptr<RtpPacket>> GetPendingFecPackets() = 0;

  /// Clear pending FEC packet queue
  virtual void ClearPendingFecPackets() = 0;

  // ========================================================================
  // Receiver: FEC Decoding
  // ========================================================================

  /// Handle received RTP packet
  /// @param packet RTP packet
  /// @return List of recovered packets (if any)
  virtual std::vector<std::shared_ptr<RtpPacket>> OnRtpPacketReceived(
      std::shared_ptr<RtpPacket> packet) = 0;

  /// Handle received FEC packet
  /// @param packet FEC packet
  virtual void OnFecPacketReceived(std::shared_ptr<RtpPacket> packet) = 0;

  /// Try to recover packet with specified sequence number
  /// @param missing_seq Missing sequence number
  /// @return Recovered packet (if possible)
  virtual std::shared_ptr<RtpPacket> TryRecoverPacket(uint16_t missing_seq) = 0;

  /// Manually trigger FEC decoding attempt
  /// @return List of recovered packets
  virtual std::vector<std::shared_ptr<RtpPacket>> TryDecodeAll() = 0;

  // ========================================================================
  // Configuration Management
  // ========================================================================

  /// Set FEC configuration
  /// @param config FEC configuration
  virtual void SetConfig(const FecConfig& config) = 0;

  /// Get current FEC configuration
  /// @return FEC configuration
  virtual FecConfig GetConfig() const = 0;

  /// Update FEC level at runtime
  /// @param level New FEC level
  virtual void UpdateFecLevel(FecLevel level) = 0;

  // ========================================================================
  // Statistics
  // ========================================================================

  /// Get FEC statistics
  /// @return Statistics
  virtual FecStatistics GetStatistics() const = 0;

  /// Reset statistics
  virtual void ResetStatistics() = 0;

  // ========================================================================
  // Callbacks
  // ========================================================================

  /// Set FEC encode complete callback
  /// @param callback Callback function
  virtual void SetOnFecEncodeCallback(OnFecEncodeCallback callback) = 0;

  /// Set FEC recover complete callback
  /// @param callback Callback function
  virtual void SetOnFecRecoverCallback(OnFecRecoverCallback callback) = 0;

  // ========================================================================
  // State Query
  // ========================================================================

  /// Check if FEC module is enabled
  /// @return true if enabled
  virtual bool IsEnabled() const = 0;

  /// Get current FEC group information
  /// @return List of FEC groups
  virtual std::vector<FecGroup> GetFecGroups() const = 0;
};

// ============================================================================
// XOR FEC Encoder/Decoder
// ============================================================================

/// XOR FEC encoder
class XorFecEncoder {
 public:
  XorFecEncoder() = default;
  ~XorFecEncoder() = default;

  /// Add media packet to encoding group
  /// @param packet Media packet
  /// @return true if added successfully
  bool AddPacket(std::shared_ptr<RtpPacket> packet);

  /// Perform XOR encoding
  /// @return Generated FEC packet
  std::shared_ptr<RtpPacket> Encode();

  /// Clear encoding group
  void Clear();

  /// Get current group size
  size_t group_size() const { return packets_.size(); }

  /// Check if can encode
  bool can_encode() const { return !packets_.empty(); }

 private:
  std::vector<std::shared_ptr<RtpPacket>> packets_;
};

/// XOR FEC decoder
class XorFecDecoder {
 public:
  XorFecDecoder() = default;
  ~XorFecDecoder() = default;

  /// Add FEC packet to decoder
  /// @param fec_packet FEC packet
  void AddFecPacket(std::shared_ptr<RtpPacket> fec_packet);

  /// Add media packet to decoder
  /// @param media_packet Media packet
  void AddMediaPacket(std::shared_ptr<RtpPacket> media_packet);

  /// Try to recover packet with specified sequence number
  /// @param seq_num Sequence number to recover
  /// @return Recovered packet (if possible)
  std::shared_ptr<RtpPacket> TryRecover(uint16_t seq_num);

  /// Try to recover all recoverable packets
  /// @return List of recovered packets
  std::vector<std::shared_ptr<RtpPacket>> RecoverAll();

  /// Clear decoder state
  void Clear();

 private:
  std::vector<std::shared_ptr<RtpPacket>> media_packets_;
  std::shared_ptr<RtpPacket> fec_packet_;
};

// ============================================================================
// FEC Module Implementation
// ============================================================================

/// FEC module implementation
class FecModule : public IFecModule {
 public:
  /// Constructor
  FecModule();
  
  /// Destructor
  ~FecModule() override = default;

  // ========================================================================
  // Lifecycle
  // ========================================================================

  /// Initialize FEC module
  bool Initialize(const FecConfig& config) override;

  /// Start FEC module
  void Start() override;

  /// Stop FEC module
  void Stop() override;

  /// Reset FEC module state
  void Reset() override;

  // ========================================================================
  // Sender: FEC Encoding
  // ========================================================================

  /// Add media packet to FEC encoder
  bool AddMediaPacket(std::shared_ptr<RtpPacket> packet) override;

  /// Trigger FEC encoding and generate redundancy packets
  std::vector<std::shared_ptr<RtpPacket>> EncodeFec() override;

  /// Get pending FEC packets to send
  std::vector<std::shared_ptr<RtpPacket>> GetPendingFecPackets() override;

  /// Clear pending FEC packet queue
  void ClearPendingFecPackets() override;

  // ========================================================================
  // Receiver: FEC Decoding
  // ========================================================================

  /// Handle received RTP packet
  std::vector<std::shared_ptr<RtpPacket>> OnRtpPacketReceived(
      std::shared_ptr<RtpPacket> packet) override;

  /// Handle received FEC packet
  void OnFecPacketReceived(std::shared_ptr<RtpPacket> packet) override;

  /// Try to recover packet with specified sequence number
  std::shared_ptr<RtpPacket> TryRecoverPacket(uint16_t missing_seq) override;

  /// Manually trigger FEC decoding attempt
  std::vector<std::shared_ptr<RtpPacket>> TryDecodeAll() override;

  // ========================================================================
  // Configuration Management
  // ========================================================================

  /// Set FEC configuration
  void SetConfig(const FecConfig& config) override;

  /// Get current FEC configuration
  FecConfig GetConfig() const override;

  /// Update FEC level at runtime
  void UpdateFecLevel(FecLevel level) override;

  // ========================================================================
  // Statistics
  // ========================================================================

  /// Get FEC statistics
  FecStatistics GetStatistics() const override;

  /// Reset statistics
  void ResetStatistics() override;

  // ========================================================================
  // Callbacks
  // ========================================================================

  /// Set FEC encode complete callback
  void SetOnFecEncodeCallback(OnFecEncodeCallback callback) override;

  /// Set FEC recover complete callback
  void SetOnFecRecoverCallback(OnFecRecoverCallback callback) override;

  // ========================================================================
  // State Query
  // ========================================================================

  /// Check if FEC module is enabled
  bool IsEnabled() const override;

  /// Get current FEC group information
  std::vector<FecGroup> GetFecGroups() const override;

 private:
  /// Create FEC packet from media packets
  std::shared_ptr<RtpPacket> CreateFecPacket(
      const std::vector<std::shared_ptr<RtpPacket>>& packets);
  
  /// Check if packet is FEC packet
  bool IsFecPacket(std::shared_ptr<RtpPacket> packet) const;
  
  /// Get protected sequence numbers from FEC packet
  std::vector<uint16_t> GetProtectedSeqNums(std::shared_ptr<RtpPacket> fec_packet) const;

  /// Configuration
  FecConfig config_;
  
  /// XOR encoder for sending
  XorFecEncoder encoder_;
  
  /// XOR decoder for receiving
  XorFecDecoder decoder_;
  
  /// Pending FEC packets to send
  std::vector<std::shared_ptr<RtpPacket>> pending_fec_packets_;
  
  /// Received media packets (for decoding)
  std::map<uint16_t, std::shared_ptr<RtpPacket>> received_media_packets_;
  
  /// Received FEC packets
  std::vector<std::shared_ptr<RtpPacket>> received_fec_packets_;
  
  /// Statistics
  FecStatistics stats_;
  
  /// Callbacks
  OnFecEncodeCallback on_fec_encode_;
  OnFecRecoverCallback on_fec_recover_;
  
  /// Internal state
  bool initialized_;
  bool running_;
  uint16_t current_group_start_seq_;
  int64_t last_group_time_ms_;
};

// ============================================================================
// Factory
// ============================================================================

/// FEC module factory
class FecModuleFactory {
 public:
  /// Create FEC module with specified algorithm
  /// @param algorithm FEC algorithm
  /// @return FEC module pointer
  static std::unique_ptr<IFecModule> Create(FecAlgorithm algorithm);

  /// Create ULP FEC module (default)
  static std::unique_ptr<IFecModule> CreateUlpFec();

  /// Create XOR FEC module
  static std::unique_ptr<IFecModule> CreateXorFec();
};

}  // namespace minirtc

#endif  // MINIRTC_FEC_MODULE_H
