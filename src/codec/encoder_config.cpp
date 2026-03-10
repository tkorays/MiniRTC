/**
 * @file encoder_config.cpp
 * @brief Encoder configuration implementation
 */

#include "minirtc/codec/encoder_config.h"
#include <sstream>
#include <iomanip>

namespace minirtc {

// ===== VideoEncoderConfig =====

std::string VideoEncoderConfig::ToString() const {
  std::ostringstream ss;
  ss << "{"
     << "\"type\":" << static_cast<int>(type) << ","
     << "\"media_type\":" << static_cast<int>(media_type) << ","
     << "\"width\":" << width << ","
     << "\"height\":" << height << ","
     << "\"framerate\":" << framerate << ","
     << "\"keyframe_interval\":" << keyframe_interval << ","
     << "\"target_bitrate_kbps\":" << target_bitrate_kbps << ","
     << "\"max_bitrate_kbps\":" << max_bitrate_kbps << ","
     << "\"bitrate_control\":" << static_cast<int>(bitrate_control) << ","
     << "\"quality\":" << static_cast<int>(quality) << ","
     << "\"profile\":\"" << profile << "\","
     << "\"level\":\"" << level << "\","
     << "\"entropy_mode\":\"" << entropy_mode << "\","
     << "\"max_bframes\":" << max_bframes << ","
     << "\"use_hardware\":" << (use_hardware ? "true" : "false") << ","
     << "\"complexity\":" << complexity
     << "}";
  return ss.str();
}

bool VideoEncoderConfig::FromString(const std::string& json) {
  // Simplified JSON parsing - in production, use a proper JSON library
  // For now, just return false to indicate not implemented
  (void)json;
  return false;
}

// ===== AudioEncoderConfig =====

std::string AudioEncoderConfig::ToString() const {
  std::ostringstream ss;
  ss << "{"
     << "\"type\":" << static_cast<int>(type) << ","
     << "\"media_type\":" << static_cast<int>(media_type) << ","
     << "\"sample_rate\":" << sample_rate << ","
     << "\"channels\":" << channels << ","
     << "\"bitrate_bps\":" << bitrate_bps << ","
     << "\"complexity\":" << complexity << ","
     << "\"application_kbe\":" << (application_kbe ? "true" : "false") << ","
     << "\"frame_size_ms\":" << frame_size_ms << ","
     << "\"bandwidth\":\"" << bandwidth << "\","
     << "\"vbr\":" << (vbr ? "true" : "false") << ","
     << "\"vbr_constraint\":" << (vbr_constraint ? "true" : "false")
     << "}";
  return ss.str();
}

bool AudioEncoderConfig::FromString(const std::string& json) {
  // Simplified JSON parsing
  (void)json;
  return false;
}

}  // namespace minirtc
