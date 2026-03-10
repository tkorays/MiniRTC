/**
 * @file encoder_config.h
 * @brief MiniRTC encoder configuration
 */

#ifndef MINIRTC_ENCODER_CONFIG_H_
#define MINIRTC_ENCODER_CONFIG_H_

#include "icodec.h"
#include <string>

namespace minirtc {

/**
 * @brief Video encoder configuration
 */
struct VideoEncoderConfig : public ICodecConfig {
  CodecType type = CodecType::kH264;
  MediaType media_type = MediaType::kVideo;
  
  // Resolution
  uint32_t width = 1280;
  uint32_t height = 720;
  
  // Frame rate
  uint32_t framerate = 30;
  uint32_t keyframe_interval = 60;  // Keyframe interval
  
  // Bitrate control
  uint32_t target_bitrate_kbps = 1000;  // Target bitrate (kbps)
  uint32_t max_bitrate_kbps = 2000;     // Max bitrate (kbps)
  BitrateControl bitrate_control = BitrateControl::kVBR;
  
  // Encode quality
  EncodeQuality quality = EncodeQuality::kMedium;
  
  // Encode profile
  std::string profile = "high";    // baseline, main, high
  std::string level = "3.1";       // 3.1, 4.0, 4.1, etc.
  
  // Entropy coding
  std::string entropy_mode = "cabac";  // cabac, cavlc
  
  // B-frame configuration
  int max_bframes = 0;
  int b_frame_ref = 0;
  
  // Hardware acceleration
  bool use_hardware = true;
  std::string hardware_device = "auto";  // auto, 0, 1, ...
  
  // Multi-threading
  int thread_count = 0;  // 0 = auto
  
  // Complexity
  int complexity = 50;   // 0-100
  
  // Scene adaptive
  bool scene_change_detection = true;
  
  // ICodecConfig implementation
  CodecType GetType() const override { return type; }
  MediaType GetMediaType() const override { return media_type; }
  
  std::unique_ptr<ICodecConfig> Clone() const override {
    return std::make_unique<VideoEncoderConfig>(*this);
  }
  
  std::string ToString() const override;
  bool FromString(const std::string& json) override;
};

/**
 * @brief Audio encoder configuration
 */
struct AudioEncoderConfig : public ICodecConfig {
  CodecType type = CodecType::kOpus;
  MediaType media_type = MediaType::kAudio;
  
  // Sample rate
  uint32_t sample_rate = 48000;
  
  // Number of channels
  uint32_t channels = 2;
  
  // Bitrate
  uint32_t bitrate_bps = 64000;  // bps
  
  // Encode quality
  EncodeQuality quality = EncodeQuality::kMedium;
  
  // Encode complexity
  int complexity = 10;  // 0-10
  
  // Signal type
  bool force_channel_mapping = false;
  bool application_kbe = false;  // true = music, false = voice
  
  // Frame size
  uint32_t frame_size_ms = 20;  // 5, 10, 20, 40, 60, 80, 100, 120
  
  // Bandwidth control
  std::string bandwidth = "fullband";  // narrowband, mediumband, wideband, superwideband, fullband
  
  // VBR configuration
  bool vbr = true;
  bool vbr_constraint = false;
  
  // Signal detection
  bool signal_detection = true;
  bool force_mode = false;  // Force use specified mode
  
  // ICodecConfig implementation
  CodecType GetType() const override { return type; }
  MediaType GetMediaType() const override { return media_type; }
  
  std::unique_ptr<ICodecConfig> Clone() const override {
    return std::make_unique<AudioEncoderConfig>(*this);
  }
  
  std::string ToString() const override;
  bool FromString(const std::string& json) override;
};

}  // namespace minirtc

#endif  // MINIRTC_ENCODER_CONFIG_H_
