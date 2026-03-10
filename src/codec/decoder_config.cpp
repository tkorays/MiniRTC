/**
 * @file decoder_config.cpp
 * @brief Decoder configuration implementation
 */

#include "minirtc/codec/decoder_config.h"
#include <sstream>

namespace minirtc {

// ===== VideoDecoderConfig =====

std::string VideoDecoderConfig::ToString() const {
  std::ostringstream ss;
  ss << "{"
     << "\"type\":" << static_cast<int>(type) << ","
     << "\"media_type\":" << static_cast<int>(media_type) << ","
     << "\"output_format\":" << static_cast<int>(output_format) << ","
     << "\"thread_count\":" << thread_count << ","
     << "\"use_hardware\":" << (use_hardware ? "true" : "false") << ","
     << "\"hardware_device\":\"" << hardware_device << "\","
     << "\"error_concealment\":" << (error_concealment ? "true" : "false") << ","
     << "\"low_latency\":" << (low_latency ? "true" : "false") << ","
     << "\"enable_frame_drop\":" << (enable_frame_drop ? "true" : "false")
     << "}";
  return ss.str();
}

bool VideoDecoderConfig::FromString(const std::string& json) {
  // Simplified JSON parsing
  (void)json;
  return false;
}

// ===== AudioDecoderConfig =====

std::string AudioDecoderConfig::ToString() const {
  std::ostringstream ss;
  ss << "{"
     << "\"type\":" << static_cast<int>(type) << ","
     << "\"media_type\":" << static_cast<int>(media_type) << ","
     << "\"output_format\":" << static_cast<int>(output_format) << ","
     << "\"output_sample_rate\":" << output_sample_rate << ","
     << "\"output_channels\":" << output_channels << ","
     << "\"packet_loss_concealment\":" << (packet_loss_concealment ? "true" : "false")
     << "}";
  return ss.str();
}

bool AudioDecoderConfig::FromString(const std::string& json) {
  // Simplified JSON parsing
  (void)json;
  return false;
}

}  // namespace minirtc
