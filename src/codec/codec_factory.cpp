/**
 * @file codec_factory.cpp
 * @brief Codec factory implementation
 */

#include "minirtc/codec/codec_factory.h"
#include "minirtc/codec/opus_encoder.h"
#include "minirtc/codec/opus_decoder.h"
#include "minirtc/codec/h264_encoder.h"
#include "minirtc/codec/h264_decoder.h"

namespace minirtc {

CodecFactory::CodecFactory() {
  RegisterDefaultCodecs();
}

CodecFactory& CodecFactory::Instance() {
  static CodecFactory factory;
  return factory;
}

void CodecFactory::RegisterEncoder(CodecType type,
                                   std::function<std::unique_ptr<IEncoder>()> creator) {
  encoder_creators_[type] = creator;
}

void CodecFactory::RegisterDecoder(CodecType type,
                                    std::function<std::unique_ptr<IDecoder>()> creator) {
  decoder_creators_[type] = creator;
}

void CodecFactory::UnregisterEncoder(CodecType type) {
  encoder_creators_.erase(type);
}

void CodecFactory::UnregisterDecoder(CodecType type) {
  decoder_creators_.erase(type);
}

std::unique_ptr<IEncoder> CodecFactory::CreateEncoder(CodecType type,
                                                      const ICodecConfig& config) {
  auto it = encoder_creators_.find(type);
  if (it == encoder_creators_.end()) {
    return nullptr;
  }
  
  auto encoder = it->second();
  if (!encoder) {
    return nullptr;
  }
  
  auto err = encoder->Initialize(config);
  if (err != CodecError::kOk) {
    return nullptr;
  }
  
  return encoder;
}

std::unique_ptr<IEncoder> CodecFactory::CreateEncoder(const VideoEncoderConfig& config) {
  return CreateEncoder(config.type, config);
}

std::unique_ptr<IEncoder> CodecFactory::CreateEncoder(const AudioEncoderConfig& config) {
  return CreateEncoder(config.type, config);
}

std::unique_ptr<IDecoder> CodecFactory::CreateDecoder(CodecType type,
                                                      const ICodecConfig& config) {
  auto it = decoder_creators_.find(type);
  if (it == decoder_creators_.end()) {
    return nullptr;
  }
  
  auto decoder = it->second();
  if (!decoder) {
    return nullptr;
  }
  
  auto err = decoder->Initialize(config);
  if (err != CodecError::kOk) {
    return nullptr;
  }
  
  return decoder;
}

std::unique_ptr<IDecoder> CodecFactory::CreateDecoder(const VideoDecoderConfig& config) {
  return CreateDecoder(config.type, config);
}

std::unique_ptr<IDecoder> CodecFactory::CreateDecoder(const AudioDecoderConfig& config) {
  return CreateDecoder(config.type, config);
}

std::vector<CodecType> CodecFactory::GetSupportedEncoders(MediaType media_type) const {
  std::vector<CodecType> result;
  
  for (const auto& [type, creator] : encoder_creators_) {
    (void)creator;
    // Determine media type based on codec type
    switch (type) {
      case CodecType::kOpus:
      case CodecType::kAAC:
      case CodecType::kG722:
      case CodecType::kPCMU:
      case CodecType::kPCMA:
        if (media_type == MediaType::kAudio || media_type == MediaType::kNone) {
          result.push_back(type);
        }
        break;
      case CodecType::kH264:
      case CodecType::kH265:
      case CodecType::kVP8:
      case CodecType::kVP9:
      case CodecType::kAV1:
        if (media_type == MediaType::kVideo || media_type == MediaType::kNone) {
          result.push_back(type);
        }
        break;
      default:
        break;
    }
  }
  
  return result;
}

std::vector<CodecType> CodecFactory::GetSupportedDecoders(MediaType media_type) const {
  std::vector<CodecType> result;
  
  for (const auto& [type, creator] : decoder_creators_) {
    (void)creator;
    // Determine media type based on codec type
    switch (type) {
      case CodecType::kOpus:
      case CodecType::kAAC:
      case CodecType::kG722:
      case CodecType::kPCMU:
      case CodecType::kPCMA:
        if (media_type == MediaType::kAudio || media_type == MediaType::kNone) {
          result.push_back(type);
        }
        break;
      case CodecType::kH264:
      case CodecType::kH265:
      case CodecType::kVP8:
      case CodecType::kVP9:
      case CodecType::kAV1:
        if (media_type == MediaType::kVideo || media_type == MediaType::kNone) {
          result.push_back(type);
        }
        break;
      default:
        break;
    }
  }
  
  return result;
}

bool CodecFactory::IsEncoderSupported(CodecType type, const ICodecConfig& config) const {
  auto it = encoder_creators_.find(type);
  if (it == encoder_creators_.end()) {
    return false;
  }
  
  auto encoder = it->second();
  if (!encoder) {
    return false;
  }
  
  return encoder->IsSupported(config);
}

bool CodecFactory::IsDecoderSupported(CodecType type, const ICodecConfig& config) const {
  auto it = decoder_creators_.find(type);
  if (it == decoder_creators_.end()) {
    return false;
  }
  
  auto decoder = it->second();
  if (!decoder) {
    return false;
  }
  
  return decoder->IsSupported(config);
}

std::unique_ptr<IEncoder> CodecFactory::CreateEncoderFromSDP(const std::string& sdp,
                                                             MediaType media_type) {
  // Simplified SDP parsing
  (void)sdp;
  
  // Default codecs based on media type
  if (media_type == MediaType::kAudio) {
    AudioEncoderConfig config;
    config.type = CodecType::kOpus;
    return CreateEncoder(config);
  } else if (media_type == MediaType::kVideo) {
    VideoEncoderConfig config;
    config.type = CodecType::kH264;
    return CreateEncoder(config);
  }
  
  return nullptr;
}

std::unique_ptr<IDecoder> CodecFactory::CreateDecoderFromSDP(const std::string& sdp,
                                                              MediaType media_type) {
  // Simplified SDP parsing
  (void)sdp;
  
  // Default codecs based on media type
  if (media_type == MediaType::kAudio) {
    AudioDecoderConfig config;
    config.type = CodecType::kOpus;
    return CreateDecoder(config);
  } else if (media_type == MediaType::kVideo) {
    VideoDecoderConfig config;
    config.type = CodecType::kH264;
    return CreateDecoder(config);
  }
  
  return nullptr;
}

void CodecFactory::RegisterDefaultCodecs() {
  // Register Opus
  RegisterEncoder(CodecType::kOpus, []() {
    return std::make_unique<OpusEncoder>();
  });
  RegisterDecoder(CodecType::kOpus, []() {
    return std::make_unique<OpusDecoder>();
  });
  
  // Register H.264
  RegisterEncoder(CodecType::kH264, []() {
    return std::make_unique<H264Encoder>();
  });
  RegisterDecoder(CodecType::kH264, []() {
    return std::make_unique<H264Decoder>();
  });
}

// ===== ICodec Static Methods =====

std::vector<CodecType> ICodec::GetSupportedCodecs(MediaType media_type) {
  auto& factory = CodecFactory::Instance();
  std::vector<CodecType> encoders = factory.GetSupportedEncoders(media_type);
  std::vector<CodecType> decoders = factory.GetSupportedDecoders(media_type);
  
  std::vector<CodecType> result;
  result.insert(result.end(), encoders.begin(), encoders.end());
  result.insert(result.end(), decoders.begin(), decoders.end());
  
  return result;
}

std::string ICodec::GetCodecName(CodecType type) {
  switch (type) {
    case CodecType::kOpus: return "Opus";
    case CodecType::kAAC: return "AAC";
    case CodecType::kG722: return "G.722";
    case CodecType::kPCMU: return "PCMU (G.711 μ-law)";
    case CodecType::kPCMA: return "PCMA (G.711 A-law)";
    case CodecType::kH264: return "H.264/AVC";
    case CodecType::kH265: return "H.265/HEVC";
    case CodecType::kVP8: return "VP8";
    case CodecType::kVP9: return "VP9";
    case CodecType::kAV1: return "AV1";
    default: return "Unknown";
  }
}

}  // namespace minirtc
