/**
 * @file opus_encoder.h
 * @brief MiniRTC Opus audio encoder
 */

#ifndef MINIRTC_OPUS_ENCODER_H_
#define MINIRTC_OPUS_ENCODER_H_

#include "iencoder.h"
#include "encoder_config.h"

#ifdef MINIRTC_USE_OPUS
#include <opus.h>
#else
// Stub definitions when Opus is not available
#include <cstdint>
typedef struct OpusEncoder OpusEncoder;
typedef struct OpusDecoder OpusDecoder;
typedef int32_t opus_int32;
typedef int16_t opus_int16;
typedef uint8_t opus_uint8;
typedef uint32_t opus_uint32;
#define OPUS_APPLICATION_VOIP 2048
#define OPUS_APPLICATION_AUDIO 2049
#define OPUS_OK 0
#define OPUS_BAD_ARG -1
#define OPUS_BUFFER_TOO_SMALL -4
#define OPUS_INVALID_PACKET -6
#endif

namespace minirtc {

/**
 * @brief Opus audio encoder
 * 
 * Based on libopus, supports VoIP and music modes
 */
class OpusEncoder : public IEncoder {
 public:
  OpusEncoder();
  ~OpusEncoder() override;
  
  // ===== ICodec Interface =====
  
  CodecError Initialize(const ICodecConfig& config) override;
  CodecError Release() override;
  CodecError Reset() override;
  
  CodecType GetType() const override { return CodecType::kOpus; }
  MediaType GetMediaType() const override { return MediaType::kAudio; }
  CodecState GetState() const override { return state_; }
  std::unique_ptr<ICodecConfig> GetConfig() const override;
  
  CodecStats GetStats() const override;
  void ResetStats() override;
  
  bool IsSupported(const ICodecConfig& config) const override;
  
  // ===== IEncoder Interface =====
  
  CodecError SetConfig(const AudioEncoderConfig& config) override;
  std::unique_ptr<AudioEncoderConfig> GetAudioConfig() const override;
  
  CodecError Encode(std::shared_ptr<RawFrame> input,
                   std::shared_ptr<EncodedFrame>* output) override;
  CodecError EncodeBatch(const std::vector<std::shared_ptr<RawFrame>>& inputs,
                        std::vector<std::shared_ptr<EncodedFrame>>* outputs) override;
  CodecError Flush(std::vector<std::shared_ptr<EncodedFrame>>* outputs) override;
  
  void RequestKeyframe() override;  // Audio has no keyframe, empty implementation
  CodecError SetBitrate(uint32_t target_bps, uint32_t max_bps) override;
  CodecError SetFramerate(uint32_t fps) override;
  CodecError SetQuality(EncodeQuality quality) override;
  
  void SetCallback(ICodecCallback* callback) override;
  
  // ===== Opus Specific Interfaces =====
  
  /// Set Opus application type
  void SetApplication(opus_int32 application);  // OPUS_APPLICATION_VOIP or OPUS_APPLICATION_AUDIO
  
  /// Set bandwidth
  void SetBandwidth(opus_int32 bandwidth);  // OPUS_BANDWIDTH_*
  
  /// Set Force Channel Mapping
  void SetForceChannelMapping(bool force);
  
  /// Get current frame size (number of samples)
  int GetFrameSize() const { return frame_size_; }
  
  /// Check if encoder is valid
  bool IsValid() const { return encoder_ != nullptr; }

 private:
  CodecError CreateEncoder();
  CodecError DestroyEncoder();
  CodecError UpdateEncoderSettings();
  
  OpusEncoder* encoder_ = nullptr;
  AudioEncoderConfig config_;
  CodecState state_ = CodecState::kUninitialized;
  CodecStats stats_;
  ICodecCallback* callback_ = nullptr;
  
  int frame_size_ = 0;
  int lookahead_ = 0;
  uint64_t timestamp_us_ = 0;
  
  // Buffers
  std::vector<uint8_t> input_buffer_;
  std::vector<uint8_t> output_buffer_;
  
  // Static counter for frame numbers
  static uint32_t frame_counter_;
};

/**
 * @brief Create Opus encoder
 */
std::unique_ptr<IEncoder> CreateOpusEncoder();

}  // namespace minirtc

#endif  // MINIRTC_OPUS_ENCODER_H_
