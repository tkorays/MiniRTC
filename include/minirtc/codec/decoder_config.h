/**
 * @file decoder_config.h
 * @brief MiniRTC decoder configuration
 */

#ifndef MINIRTC_DECODER_CONFIG_H_
#define MINIRTC_DECODER_CONFIG_H_

#include "icodec.h"

namespace minirtc {

/**
 * @brief Video decoder configuration
 */
struct VideoDecoderConfig : public ICodecConfig {
  CodecType type = CodecType::kH264;
  MediaType media_type = MediaType::kVideo;
  
  // Resolution (for initialization)
  uint32_t width = 1280;
  uint32_t height = 720;
  
  // Output format
  VideoPixelFormat output_format = VideoPixelFormat::kI420;
  
  // Thread configuration
  int thread_count = 0;  // 0 = auto
  
  // Hardware acceleration
  bool use_hardware = true;
  std::string hardware_device = "auto";
  
  // Error concealment
  bool error_concealment = true;
  
  // Low latency mode
  bool low_latency = false;
  
  // Error recovery
  bool enable_frame_drop = true;
  
  // ICodecConfig implementation
  CodecType GetType() const override { return type; }
  MediaType GetMediaType() const override { return media_type; }
  
  std::unique_ptr<ICodecConfig> Clone() const override {
    return std::make_unique<VideoDecoderConfig>(*this);
  }
  
  std::string ToString() const override;
  bool FromString(const std::string& json) override;
};

/**
 * @brief Audio decoder configuration
 */
struct AudioDecoderConfig : public ICodecConfig {
  CodecType type = CodecType::kOpus;
  MediaType media_type = MediaType::kAudio;
  
  // Sample rate (for initialization)
  uint32_t sample_rate = 48000;
  
  // Number of channels (for initialization)
  uint32_t channels = 2;
  
  // Output format
  AudioSampleFormat output_format = AudioSampleFormat::kInt16;
  
  // Output sample rate (0 = same as input)
  uint32_t output_sample_rate = 0;
  
  // Output number of channels (0 = same as input)
  uint32_t output_channels = 0;
  
  // Channel remapping
  std::vector<int> channel_mapping;
  
  // Packet loss concealment
  bool packet_loss_concealment = true;
  
  // ICodecConfig implementation
  CodecType GetType() const override { return type; }
  MediaType GetMediaType() const override { return media_type; }
  
  std::unique_ptr<ICodecConfig> Clone() const override {
    return std::make_unique<AudioDecoderConfig>(*this);
  }
  
  std::string ToString() const override;
  bool FromString(const std::string& json) override;
};

}  // namespace minirtc

#endif  // MINIRTC_DECODER_CONFIG_H_
