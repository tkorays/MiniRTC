/**
 * @file icodec.h
 * @brief MiniRTC codec base interface
 */

#ifndef MINIRTC_ICODEC_H_
#define MINIRTC_ICODEC_H_

#include "codec_types.h"
#include <memory>
#include <string>
#include <vector>

namespace minirtc {

/**
 * @brief Codec configuration base class
 */
class ICodecConfig {
 public:
  virtual ~ICodecConfig() = default;
  
  /// Get codec type
  virtual CodecType GetType() const = 0;
  
  /// Get media type
  virtual MediaType GetMediaType() const = 0;
  
  /// Clone configuration
  virtual std::unique_ptr<ICodecConfig> Clone() const = 0;
  
  /// Convert to JSON string
  virtual std::string ToString() const = 0;
  
  /// Parse from JSON string
  virtual bool FromString(const std::string& json) = 0;
};

/**
 * @brief Codec callback interface
 */
class ICodecCallback {
 public:
  virtual ~ICodecCallback() = default;
  
  /// Encoding completed callback
  virtual void OnEncoded(std::shared_ptr<class EncodedFrame> frame) = 0;
  
  /// Decoding completed callback
  virtual void OnDecoded(std::shared_ptr<class RawFrame> frame) = 0;
  
  /// Error callback
  virtual void OnError(CodecError error, const std::string& message) = 0;
  
  /// Statistics callback
  virtual void OnStats(const CodecStats& stats) = 0;
};

/**
 * @brief Base codec interface
 * 
 * All codec implementations must inherit from this interface
 */
class ICodec {
 public:
  using Ptr = std::shared_ptr<ICodec>;
  
  virtual ~ICodec() = default;
  
  // ===== Basic Operations =====
  
  /// Initialize codec
  virtual CodecError Initialize(const ICodecConfig& config) = 0;
  
  /// Release codec resources
  virtual CodecError Release() = 0;
  
  /// Reset codec state
  virtual CodecError Reset() = 0;
  
  // ===== State Query =====
  
  /// Get codec type
  virtual CodecType GetType() const = 0;
  
  /// Get media type
  virtual MediaType GetMediaType() const = 0;
  
  /// Get current state
  virtual CodecState GetState() const = 0;
  
  /// Get configuration
  virtual std::unique_ptr<ICodecConfig> GetConfig() const = 0;
  
  // ===== Statistics =====
  
  /// Get statistics
  virtual CodecStats GetStats() const = 0;
  
  /// Reset statistics
  virtual void ResetStats() = 0;
  
  // ===== Capability Query =====
  
  /// Check if specified configuration is supported
  virtual bool IsSupported(const ICodecConfig& config) const = 0;
  
  /// Get supported codec list
  static std::vector<CodecType> GetSupportedCodecs(MediaType media_type);
  
  /// Get codec name
  static std::string GetCodecName(CodecType type);
  
 protected:
  ICodec() = default;
  
  // Disable copy
  ICodec(const ICodec&) = delete;
  ICodec& operator=(const ICodec&) = delete;
};

}  // namespace minirtc

#endif  // MINIRTC_ICODEC_H_
