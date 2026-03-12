/**
 * @file codec_types.h
 * @brief MiniRTC codec module type definitions
 */

#ifndef MINIRTC_CODEC_TYPES_H_
#define MINIRTC_CODEC_TYPES_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <functional>

// Use shared types from capture_render_types
#include "minirtc/capture_render_types.h"

namespace minirtc {

// Remove duplicate enum definitions - using capture_render_types.h versions
// VideoPixelFormat and AudioSampleFormat are now defined in capture_render_types.h
enum class CodecType {
  kNone = 0,
  // Audio codecs
  kOpus = 1,
  kAAC = 2,
  kG722 = 3,
  kPCMU = 4,
  kPCMA = 5,
  // Video codecs
  kH264 = 100,
  kH265 = 101,
  kVP8 = 102,
  kVP9 = 103,
  kAV1 = 104,
};

/// Media type enumeration - only define if not already defined
#ifndef MINIRTC_MEDIATYPE_DEFINED
#define MINIRTC_MEDIATYPE_DEFINED
enum class MediaType {
  kNone = 0,
  kAudio = 1,
  kVideo = 2,
};
#endif

/// Codec state enumeration
enum class CodecState {
  kUninitialized = 0,
  kInitialized = 1,
  kRunning = 2,
  kPaused = 3,
  kStopped = 4,
  kError = 5,
};

/// Encode quality level
enum class EncodeQuality {
  kLow = 0,       // Low bitrate, suitable for weak networks
  kMedium = 1,    // Medium bitrate, balanced quality and bandwidth
  kHigh = 2,      // High bitrate, high quality
  kUltra = 3,     // Ultra high quality
};

/// Bitrate control mode
enum class BitrateControl {
  kCBR = 0,       // Constant bitrate
  kVBR = 1,       // Variable bitrate
  kCBRHQ = 2,     // High quality constant bitrate
  kVBRHQ = 3,     // High quality variable bitrate
};

// Use VideoPixelFormat and AudioSampleFormat from capture_render_types.h

/// Codec error code
enum class CodecError {
  kOk = 0,
  kInvalidParam = -1,
  kNotInitialized = -2,
  kAlreadyInitialized = -3,
  kNotSupported = -4,
  kOutOfMemory = -5,
  kHardwareError = -6,
  kStreamError = -7,
  kBufferTooSmall = -8,
  kTimeout = -9,
};

/// Video frame information
struct VideoFrameInfo {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t stride[4] = {0, 0, 0, 0};  // YUV plane strides
  VideoPixelFormat format = VideoPixelFormat::kI420;
  uint64_t timestamp_us = 0;         // Microsecond timestamp
  bool keyframe = false;
  int rotation = 0;                  // Rotation angle (0, 90, 180, 270)
};

/// Audio frame information
struct AudioFrameInfo {
  uint32_t sample_rate = 48000;
  uint32_t channels = 2;
  uint32_t samples_per_channel = 0;
  AudioSampleFormat format = AudioSampleFormat::kInt16;
  uint64_t timestamp_us = 0;
  bool speech = false;               // Is speech signal
  bool music = false;                // Is music signal
};

/// Codec statistics
struct CodecStats {
  uint64_t encoded_frames = 0;
  uint64_t decoded_frames = 0;
  uint64_t encoded_bytes = 0;
  uint64_t decoded_bytes = 0;
  uint32_t encode_time_us = 0;       // Encode time in microseconds
  uint32_t decode_time_us = 0;       // Decode time in microseconds
  uint32_t last_bitrate_kbps = 0;
  double encode_fps = 0.0;
  double decode_fps = 0.0;
};

}  // namespace minirtc

#endif  // MINIRTC_CODEC_TYPES_H_
