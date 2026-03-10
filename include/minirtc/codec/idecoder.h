/**
 * @file idecoder.h
 * @brief MiniRTC decoder interface
 */

#ifndef MINIRTC_IDECODER_H_
#define MINIRTC_IDECODER_H_

#include "icodec.h"
#include "encoder_frame.h"
#include "decoder_config.h"

namespace minirtc {

/**
 * @brief Decoder interface
 * 
 * All decoder implementations must inherit from this interface
 */
class IDecoder : public ICodec {
 public:
  using Ptr = std::shared_ptr<IDecoder>;
  
  // ===== Configuration =====
  
  /// Set video configuration (runtime adjustable)
  virtual CodecError SetConfig(const VideoDecoderConfig& config) {
    (void)config;
    return CodecError::kNotSupported;
  }
  
  /// Set audio configuration (runtime adjustable)
  virtual CodecError SetConfig(const AudioDecoderConfig& config) {
    (void)config;
    return CodecError::kNotSupported;
  }
  
  /// Get video configuration
  virtual std::unique_ptr<VideoDecoderConfig> GetVideoConfig() const {
    return nullptr;
  }
  
  /// Get audio configuration
  virtual std::unique_ptr<AudioDecoderConfig> GetAudioConfig() const {
    return nullptr;
  }
  
  // ===== Decoding Operations =====
  
  /// Decode single frame
  /// @param input Input encoded frame
  /// @param output Output raw frame
  /// @return Error code
  virtual CodecError Decode(std::shared_ptr<EncodedFrame> input,
                           std::shared_ptr<RawFrame>* output) = 0;
  
  /// Decode multiple frames (batch processing)
  virtual CodecError DecodeBatch(const std::vector<std::shared_ptr<EncodedFrame>>& inputs,
                                 std::vector<std::shared_ptr<RawFrame>>* outputs) = 0;
  
  /// Decode raw data (e.g., Annex-B format)
  virtual CodecError DecodeRaw(const uint8_t* data, size_t size,
                               std::shared_ptr<RawFrame>* output) = 0;
  
  /// Flush decoder (get all pending decoded data)
  virtual CodecError Flush(std::vector<std::shared_ptr<RawFrame>>* outputs) = 0;
  
  // ===== Decoder State =====
  
  /// Set SPS/PPS (H.264)
  virtual CodecError SetParameterSets(const uint8_t* sps, size_t sps_size,
                                      const uint8_t* pps, size_t pps_size) {
    (void)sps;
    (void)sps_size;
    (void)pps;
    (void)pps_size;
    return CodecError::kNotSupported;
  }
  
  /// Get current SPS
  virtual std::vector<uint8_t> GetSPS() const {
    return {};
  }
  
  /// Get current PPS
  virtual std::vector<uint8_t> GetPPS() const {
    return {};
  }
  
  /// Check if SPS/PPS has been received
  virtual bool HasParameterSets() const {
    return false;
  }
  
  // ===== Packet Loss Handling =====
  
  /// Set packet loss rate (for PLC)
  virtual void SetPacketLossRate(double loss_rate) {
    (void)loss_rate;
  }
  
  /// Notify packet lost
  virtual void NotifyPacketLost(uint16_t seq_start, uint16_t seq_end) {
    (void)seq_start;
    (void)seq_end;
  }
  
  // ===== Callback =====
  
  /// Set decode callback
  virtual void SetCallback(ICodecCallback* callback) = 0;
  
  // ===== Capability Query =====
  
  /// Get supported resolutions
  virtual void GetSupportedResolutions(std::vector<std::pair<uint32_t, uint32_t>>* resolutions) const {
    if (resolutions) resolutions->clear();
  }
  
  /// Check if hardware acceleration is available
  virtual bool IsHardwareAccelerationAvailable() const {
    return false;
  }
  
 protected:
  IDecoder() = default;
};

}  // namespace minirtc

#endif  // MINIRTC_IDECODER_H_
