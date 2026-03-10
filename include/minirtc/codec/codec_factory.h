/**
 * @file codec_factory.h
 * @brief MiniRTC codec factory
 */

#ifndef MINIRTC_CODEC_FACTORY_H_
#define MINIRTC_CODEC_FACTORY_H_

#include "icodec.h"
#include "iencoder.h"
#include "idecoder.h"
#include "encoder_config.h"
#include "decoder_config.h"

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace minirtc {

/**
 * @brief Codec factory
 * 
 * Responsible for creating and managing codec instances
 */
class CodecFactory {
 public:
  using Ptr = std::shared_ptr<CodecFactory>;
  
  /**
   * @brief Get factory singleton
   */
  static CodecFactory& Instance();
  
  // ===== Register/Unregister Codecs =====
  
  /// Register encoder
  /// @param type Codec type
  /// @param creator Creator function
  void RegisterEncoder(CodecType type, 
                      std::function<std::unique_ptr<IEncoder>()> creator);
  
  /// Register decoder
  void RegisterDecoder(CodecType type,
                      std::function<std::unique_ptr<IDecoder>()> creator);
  
  /// Unregister encoder
  void UnregisterEncoder(CodecType type);
  
  /// Unregister decoder
  void UnregisterDecoder(CodecType type);
  
  // ===== Create Codecs =====
  
  /// Create encoder
  /// @param type Codec type
  /// @param config Encoder configuration
  /// @return Codec instance, nullptr on failure
  std::unique_ptr<IEncoder> CreateEncoder(CodecType type,
                                          const ICodecConfig& config);
  
  /// Create encoder (via config struct)
  std::unique_ptr<IEncoder> CreateEncoder(const VideoEncoderConfig& config);
  std::unique_ptr<IEncoder> CreateEncoder(const AudioEncoderConfig& config);
  
  /// Create decoder
  std::unique_ptr<IDecoder> CreateDecoder(CodecType type,
                                          const ICodecConfig& config);
  std::unique_ptr<IDecoder> CreateDecoder(const VideoDecoderConfig& config);
  std::unique_ptr<IDecoder> CreateDecoder(const AudioDecoderConfig& config);
  
  // ===== Capability Query =====
  
  /// Get supported encoder list
  std::vector<CodecType> GetSupportedEncoders(MediaType media_type) const;
  
  /// Get supported decoder list
  std::vector<CodecType> GetSupportedDecoders(MediaType media_type) const;
  
  /// Check if encoder supports specified configuration
  bool IsEncoderSupported(CodecType type, const ICodecConfig& config) const;
  
  /// Check if decoder supports specified configuration
  bool IsDecoderSupported(CodecType type, const ICodecConfig& config) const;
  
  // ===== Convenience Methods =====
  
  /// Create encoder from SDP
  std::unique_ptr<IEncoder> CreateEncoderFromSDP(const std::string& sdp,
                                                 MediaType media_type);
  
  /// Create decoder from SDP
  std::unique_ptr<IDecoder> CreateDecoderFromSDP(const std::string& sdp,
                                                MediaType media_type);
  
 private:
  CodecFactory();
  ~CodecFactory() = default;
  
  // Disable copy
  CodecFactory(const CodecFactory&) = delete;
  CodecFactory& operator=(const CodecFactory&) = delete;
  
  // Register default codecs
  void RegisterDefaultCodecs();
  
  // Codec creator function maps
  std::map<CodecType, std::function<std::unique_ptr<IEncoder>()>> encoder_creators_;
  std::map<CodecType, std::function<std::unique_ptr<IDecoder>()>> decoder_creators_;
};

/**
 * @brief Codec auto-registration template
 * 
 * Used for auto-registering codecs
 */
template<CodecType Type, typename EncoderImpl, typename DecoderImpl>
class CodecRegistrar {
 public:
  CodecRegistrar() {
    CodecFactory::Instance().RegisterEncoder(Type, []() {
      return std::make_unique<EncoderImpl>();
    });
    CodecFactory::Instance().RegisterDecoder(Type, []() {
      return std::make_unique<DecoderImpl>();
    });
  }
};

/**
 * @brief Convenience macro: Register codec
 * 
 * Usage example:
 *   MINIRTC_REGISTER_CODEC(CodecType::kOpus, OpusEncoder, OpusDecoder);
 */
#define MINIRTC_REGISTER_CODEC(Type, EncoderClass, DecoderClass) \
  static ::minirtc::CodecRegistrar<Type, EncoderClass, DecoderClass> \
      g_##EncoderClass##_##DecoderClass##_registrar

}  // namespace minirtc

#endif  // MINIRTC_CODEC_FACTORY_H_
