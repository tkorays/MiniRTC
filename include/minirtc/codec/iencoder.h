/**
 * @file iencoder.h
 * @brief MiniRTC encoder interface
 */

#ifndef MINIRTC_IENCODER_H_
#define MINIRTC_IENCODER_H_

#include "icodec.h"
#include "encoder_frame.h"
#include "encoder_config.h"

namespace minirtc {

/**
 * @brief Encoder interface
 * 
 * All encoder implementations must inherit from this interface
 */
class IEncoder : public ICodec {
 public:
  using Ptr = std::shared_ptr<IEncoder>;
  
  // ===== Configuration =====
  
  /// Set video configuration (runtime adjustable)
  virtual CodecError SetConfig(const VideoEncoderConfig& config) {
    (void)config;
    return CodecError::kNotSupported;
  }
  
  /// Set audio configuration (runtime adjustable)
  virtual CodecError SetConfig(const AudioEncoderConfig& config) {
    (void)config;
    return CodecError::kNotSupported;
  }
  
  /// Get video configuration
  virtual std::unique_ptr<VideoEncoderConfig> GetVideoConfig() const {
    return nullptr;
  }
  
  /// Get audio configuration
  virtual std::unique_ptr<AudioEncoderConfig> GetAudioConfig() const {
    return nullptr;
  }
  
  // ===== Encoding Operations =====
  
  /// Encode single frame
  /// @param input Input raw frame
  /// @param output Output encoded frame
  /// @return Error code
  virtual CodecError Encode(std::shared_ptr<RawFrame> input,
                           std::shared_ptr<EncodedFrame>* output) = 0;
  
  /// Encode multiple frames (batch processing)
  virtual CodecError EncodeBatch(const std::vector<std::shared_ptr<RawFrame>>& inputs,
                                 std::vector<std::shared_ptr<EncodedFrame>>* outputs) = 0;
  
  /// Flush encoder (get all pending encoded data)
  virtual CodecError Flush(std::vector<std::shared_ptr<EncodedFrame>>* outputs) = 0;
  
  // ===== Rate Control =====
  
  /// Request keyframe
  virtual void RequestKeyframe() = 0;
  
  /// Set target bitrate
  virtual CodecError SetBitrate(uint32_t target_kbps, uint32_t max_kbps) = 0;
  
  /// Set frame rate
  virtual CodecError SetFramerate(uint32_t fps) = 0;
  
  /// Set encode quality
  virtual CodecError SetQuality(EncodeQuality quality) = 0;
  
  // ===== Callback =====
  
  /// Set encode callback
  virtual void SetCallback(ICodecCallback* callback) = 0;
  
  // ===== Capability Query =====
  
  /// Get supported resolutions
  virtual void GetSupportedResolutions(std::vector<std::pair<uint32_t, uint32_t>>* resolutions) const {
    if (resolutions) resolutions->clear();
  }
  
  /// Get supported max frame rate
  virtual uint32_t GetMaxFramerate() const {
    return 30;
  }
  
  /// Get supported max bitrate (kbps)
  virtual uint32_t GetMaxBitrate() const {
    return 2000;
  }
  
  /// Check if hardware acceleration is available
  virtual bool IsHardwareAccelerationAvailable() const {
    return false;
  }
  
 protected:
  IEncoder() = default;
};

}  // namespace minirtc

#endif  // MINIRTC_IENCODER_H_
