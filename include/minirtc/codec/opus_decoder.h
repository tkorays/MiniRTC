/**
 * @file opus_decoder.h
 * @brief MiniRTC Opus audio decoder
 */

#ifndef MINIRTC_OPUS_DECODER_H_
#define MINIRTC_OPUS_DECODER_H_

#include "idecoder.h"
#include "decoder_config.h"

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
#define OPUS_OK 0
#define OPUS_BAD_ARG -1
#define OPUS_BUFFER_TOO_SMALL -4
#define OPUS_INVALID_PACKET -6
#endif

namespace minirtc {

/**
 * @brief Opus audio decoder
 * 
 * Based on libopus, supports PLC (Packet Loss Concealment)
 */
class OpusDecoder : public IDecoder {
 public:
  OpusDecoder();
  ~OpusDecoder() override;
  
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
  
  // ===== IDecoder Interface =====
  
  CodecError SetConfig(const AudioDecoderConfig& config) override;
  std::unique_ptr<AudioDecoderConfig> GetAudioConfig() const override;
  
  CodecError Decode(std::shared_ptr<EncodedFrame> input,
                   std::shared_ptr<RawFrame>* output) override;
  CodecError DecodeBatch(const std::vector<std::shared_ptr<EncodedFrame>>& inputs,
                        std::vector<std::shared_ptr<RawFrame>>* outputs) override;
  CodecError DecodeRaw(const uint8_t* data, size_t size,
                      std::shared_ptr<RawFrame>* output) override;
  CodecError Flush(std::vector<std::shared_ptr<RawFrame>>* outputs) override;
  
  CodecError SetParameterSets(const uint8_t* sps, size_t sps_size,
                             const uint8_t* pps, size_t pps_size) override;
  std::vector<uint8_t> GetSPS() const override { return {}; }  // Audio has no SPS
  std::vector<uint8_t> GetPPS() const override { return {}; }  // Audio has no PPS
  bool HasParameterSets() const override { return true; }
  
  void SetPacketLossRate(double loss_rate) override;
  void NotifyPacketLost(uint16_t seq_start, uint16_t seq_end) override;
  
  void SetCallback(ICodecCallback* callback) override;
  
  // ===== Opus Specific Interfaces =====
  
  /// Get decoder final range
  int GetOpusFinalRange() const;
  
  /// Get current bandwidth
  int GetBandwidth() const;
  
  /// Get sample rate
  int GetSampleRate() const { return config_.sample_rate; }
  
  /// Set PLC mode
  void SetPLCEnabled(bool enabled);
  
  /// Check if decoder is valid
  bool IsValid() const { return decoder_ != nullptr; }

 private:
  CodecError CreateDecoder();
  CodecError DestroyDecoder();
  
  AudioDecoderConfig config_;
  OpusDecoder* decoder_ = nullptr;
  CodecState state_ = CodecState::kUninitialized;
  CodecStats stats_;
  ICodecCallback* callback_ = nullptr;
  
  // PLC state
  bool plc_enabled_ = true;
  double packet_loss_rate_ = 0.0;
  
  // Output buffer
  std::vector<int16_t> output_buffer_;
  int output_samples_ = 0;
  
  // State tracking
  int last_seq_ = -1;
  
  // Static counter for frame numbers
  static uint32_t frame_counter_;
};

/**
 * @brief Create Opus decoder
 */
std::unique_ptr<IDecoder> CreateOpusDecoder();

}  // namespace minirtc

#endif  // MINIRTC_OPUS_DECODER_H_
