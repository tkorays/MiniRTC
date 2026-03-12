/**
 * @file opus_decoder.cpp
 * @brief Opus decoder implementation
 */

#include "minirtc/codec/opus_decoder.h"
#include "minirtc/codec/encoder_frame.h"

#ifdef MINIRTC_USE_OPUS
#include <opus.h>
#endif

namespace minirtc {

uint32_t OpusDecoder::frame_counter_ = 0;

OpusDecoder::OpusDecoder() {
}

OpusDecoder::~OpusDecoder() {
  Release();
}

CodecError OpusDecoder::Initialize(const ICodecConfig& config) {
  if (state_ == CodecState::kInitialized || state_ == CodecState::kRunning) {
    return CodecError::kAlreadyInitialized;
  }
  
  const AudioDecoderConfig* audio_config = dynamic_cast<const AudioDecoderConfig*>(&config);
  if (!audio_config) {
    return CodecError::kInvalidParam;
  }
  
  config_ = *audio_config;
  return CreateDecoder();
}

CodecError OpusDecoder::Release() {
  DestroyDecoder();
  state_ = CodecState::kStopped;
  return CodecError::kOk;
}

CodecError OpusDecoder::Reset() {
  auto err = Release();
  if (err != CodecError::kOk) {
    return err;
  }
  return CreateDecoder();
}

std::unique_ptr<ICodecConfig> OpusDecoder::GetConfig() const {
  return std::make_unique<AudioDecoderConfig>(config_);
}

CodecStats OpusDecoder::GetStats() const {
  return stats_;
}

void OpusDecoder::ResetStats() {
  stats_ = CodecStats();
}

bool OpusDecoder::IsSupported(const ICodecConfig& config) const {
  const AudioDecoderConfig* audio_config = dynamic_cast<const AudioDecoderConfig*>(&config);
  if (!audio_config) {
    return false;
  }
  
  // Check if it's Opus
  if (audio_config->type != CodecType::kOpus) {
    return false;
  }
  
  return true;
}

CodecError OpusDecoder::SetConfig(const AudioDecoderConfig& config) {
  if (config.type != CodecType::kOpus) {
    return CodecError::kInvalidParam;
  }
  config_ = config;
  return CodecError::kOk;
}

std::unique_ptr<AudioDecoderConfig> OpusDecoder::GetAudioConfig() const {
  return std::make_unique<AudioDecoderConfig>(config_);
}

CodecError OpusDecoder::Decode(std::shared_ptr<EncodedFrame> input,
                               std::shared_ptr<RawFrame>* output) {
  if (!input || !output) {
    return CodecError::kInvalidParam;
  }
  
  if (state_ != CodecState::kRunning) {
    return CodecError::kNotInitialized;
  }
  
#ifdef MINIRTC_USE_OPUS
  if (!decoder_) {
    return CodecError::kNotInitialized;
  }
  
  const uint8_t* input_data = input->GetData();
  size_t input_size = input->GetSize();
  
  if (input_size == 0 || !input_data) {
    return CodecError::kInvalidParam;
  }
  
  // Determine output sample rate and channels
  int output_sample_rate = config_.output_sample_rate > 0 ? 
                          config_.output_sample_rate : 48000;
  int output_channels = config_.output_channels > 0 ? 
                       config_.output_channels : 2;
  
  // Calculate max output samples (120ms max frame)
  int max_output_samples = static_cast<int>(output_sample_rate * 120 / 1000);
  output_buffer_.resize(max_output_samples * output_channels);
  
  // Decode
  int decoded_samples = opus_decode(decoder_,
                                    input_data,
                                    static_cast<opus_int32>(input_size),
                                    reinterpret_cast<opus_int16*>(output_buffer_.data()),
                                    max_output_samples,
                                    0);
  
  if (decoded_samples < 0) {
    return CodecError::kStreamError;
  }
  
  output_samples_ = decoded_samples;
  
  // Create output frame
  auto decoded = std::make_shared<RawFrameImpl>();
  AudioFrameInfo info;
  info.sample_rate = output_sample_rate;
  info.channels = output_channels;
  info.samples_per_channel = decoded_samples;
  info.format = AudioSampleFormat::kInt16;
  info.timestamp_us = input->GetTimestampUs();
  decoded->SetAudioInfo(info);
  decoded->SetData(reinterpret_cast<const uint8_t*>(output_buffer_.data()),
                  decoded_samples * output_channels * sizeof(int16_t));
  decoded->SetTimestampUs(input->GetTimestampUs());
  
  // Update stats
  stats_.decoded_frames++;
  stats_.decoded_bytes += input_size;
  
  *output = decoded;
  return CodecError::kOk;
  
#else
  // Stub implementation
  auto decoded = std::make_shared<RawFrameImpl>();
  AudioFrameInfo info;
  info.sample_rate = config_.sample_rate;
  info.channels = config_.channels;
  info.samples_per_channel = 480;  // 10ms at 48kHz
  info.timestamp_us = input->GetTimestampUs();
  decoded->SetAudioInfo(info);
  decoded->SetTimestampUs(input->GetTimestampUs());
  
  stats_.decoded_frames++;
  
  *output = decoded;
  return CodecError::kOk;
#endif
}

CodecError OpusDecoder::DecodeBatch(const std::vector<std::shared_ptr<EncodedFrame>>& inputs,
                                   std::vector<std::shared_ptr<RawFrame>>* outputs) {
  if (!outputs) {
    return CodecError::kInvalidParam;
  }
  
  outputs->clear();
  outputs->reserve(inputs.size());
  
  for (const auto& input : inputs) {
    std::shared_ptr<RawFrame> output;
    auto err = Decode(input, &output);
    if (err != CodecError::kOk) {
      return err;
    }
    outputs->push_back(output);
  }
  
  return CodecError::kOk;
}

CodecError OpusDecoder::DecodeRaw(const uint8_t* data, size_t size,
                                  std::shared_ptr<RawFrame>* output) {
  if (!data || size == 0 || !output) {
    return CodecError::kInvalidParam;
  }
  
  // Create a temporary encoded frame and decode
  auto encoded = std::make_shared<EncodedFrameImpl>(MediaType::kAudio);
  encoded->SetData(data, size);
  
  return Decode(encoded, output);
}

CodecError OpusDecoder::Flush(std::vector<std::shared_ptr<RawFrame>>* outputs) {
  // No pending frames in Opus decoder
  if (outputs) {
    outputs->clear();
  }
  return CodecError::kOk;
}

CodecError OpusDecoder::SetParameterSets(const uint8_t* sps, size_t sps_size,
                                        const uint8_t* pps, size_t pps_size) {
  // Audio doesn't have SPS/PPS
  (void)sps;
  (void)sps_size;
  (void)pps;
  (void)pps_size;
  return CodecError::kOk;
}

void OpusDecoder::SetPacketLossRate(double loss_rate) {
  packet_loss_rate_ = loss_rate;
#ifdef MINIRTC_USE_OPUS
  if (decoder_) {
    opus_decoder_ctl(decoder_, OPUS_SET_PACKET_LOSS_PERC(static_cast<int>(loss_rate * 100)));
  }
#endif
}

void OpusDecoder::NotifyPacketLost(uint16_t seq_start, uint16_t seq_end) {
  // Check for gaps in sequence numbers
  if (last_seq_ >= 0 && seq_start != static_cast<uint16_t>(last_seq_ + 1)) {
    // Packet loss detected, use PLC
    if (plc_enabled_) {
#ifdef MINIRTC_USE_OPUS
      // Decode PLC
      int max_samples = 4800;  // 100ms at 48kHz
      output_buffer_.resize(max_samples * config_.channels);
      
      int decoded = opus_decode(decoder_, nullptr, 0,
                                reinterpret_cast<opus_int16*>(output_buffer_.data()),
                                max_samples, 1);
      if (decoded > 0) {
        // Could emit PLC frame here
        (void)decoded;
      }
#endif
    }
  }
  last_seq_ = seq_end;
}

void OpusDecoder::SetCallback(ICodecCallback* callback) {
  callback_ = callback;
}

int OpusDecoder::GetOpusFinalRange() const {
#ifdef MINIRTC_USE_OPUS
  if (decoder_) {
    opus_uint32 final_range;
    opus_decoder_ctl(decoder_, OPUS_GET_FINAL_RANGE(&final_range));
    return static_cast<int>(final_range);
  }
#endif
  return 0;
}

int OpusDecoder::GetBandwidth() const {
#ifdef MINIRTC_USE_OPUS
  if (decoder_) {
    opus_int32 bandwidth;
    opus_decoder_ctl(decoder_, OPUS_GET_BANDWIDTH(&bandwidth));
    return bandwidth;
  }
#endif
  return 0;
}

void OpusDecoder::SetPLCEnabled(bool enabled) {
  plc_enabled_ = enabled;
}

CodecError OpusDecoder::CreateDecoder() {
#ifdef MINIRTC_USE_OPUS
  if (decoder_) {
    opus_decoder_destroy(decoder_);
    decoder_ = nullptr;
  }
  
  int error;
  decoder_ = opus_decoder_create(config_.sample_rate > 0 ? config_.sample_rate : 48000,
                                  config_.channels > 0 ? config_.channels : 2,
                                  &error);
  
  if (error != OPUS_OK || !decoder_) {
    state_ = CodecState::kError;
    return CodecError::kOutOfMemory;
  }
  
  state_ = CodecState::kInitialized;
  return CodecError::kOk;
  
#else
  // Stub implementation
  decoder_ = reinterpret_cast<OpusDecoder*>(0x1);  // Mark as valid
  state_ = CodecState::kInitialized;
  return CodecError::kOk;
#endif
}

CodecError OpusDecoder::DestroyDecoder() {
#ifdef MINIRTC_USE_OPUS
  if (decoder_) {
    opus_decoder_destroy(decoder_);
    decoder_ = nullptr;
  }
#endif
  state_ = CodecState::kUninitialized;
  return CodecError::kOk;
}

// ===== Factory Function =====

std::unique_ptr<IDecoder> CreateOpusDecoder() {
  return std::make_unique<OpusDecoder>();
}

}  // namespace minirtc
