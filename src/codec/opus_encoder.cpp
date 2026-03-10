/**
 * @file opus_encoder.cpp
 * @brief Opus encoder implementation
 */

#include "minirtc/codec/opus_encoder.h"
#include "minirtc/codec/encoder_frame.h"

#ifdef MINIRTC_USE_OPUS
#include <opus.h>
#endif

namespace minirtc {

uint32_t OpusEncoder::frame_counter_ = 0;

OpusEncoder::OpusEncoder() {
}

OpusEncoder::~OpusEncoder() {
  Release();
}

CodecError OpusEncoder::Initialize(const ICodecConfig& config) {
  if (state_ == CodecState::kInitialized || state_ == CodecState::kRunning) {
    return CodecError::kAlreadyInitialized;
  }
  
  const AudioEncoderConfig* audio_config = dynamic_cast<const AudioEncoderConfig*>(&config);
  if (!audio_config) {
    return CodecError::kInvalidParam;
  }
  
  config_ = *audio_config;
  return CreateEncoder();
}

CodecError OpusEncoder::Release() {
  DestroyEncoder();
  state_ = CodecState::kStopped;
  return CodecError::kOk;
}

CodecError OpusEncoder::Reset() {
  auto err = Release();
  if (err != CodecError::kOk) {
    return err;
  }
  return CreateEncoder();
}

std::unique_ptr<ICodecConfig> OpusEncoder::GetConfig() const {
  return std::make_unique<AudioEncoderConfig>(config_);
}

CodecStats OpusEncoder::GetStats() const {
  return stats_;
}

void OpusEncoder::ResetStats() {
  stats_ = CodecStats();
}

bool OpusEncoder::IsSupported(const ICodecConfig& config) const {
  const AudioEncoderConfig* audio_config = dynamic_cast<const AudioEncoderConfig*>(&config);
  if (!audio_config) {
    return false;
  }
  
  // Check if it's Opus
  if (audio_config->type != CodecType::kOpus) {
    return false;
  }
  
  // Check sample rate
  if (audio_config->sample_rate != 8000 && 
      audio_config->sample_rate != 12000 &&
      audio_config->sample_rate != 16000 &&
      audio_config->sample_rate != 24000 &&
      audio_config->sample_rate != 48000) {
    return false;
  }
  
  // Check channels
  if (audio_config->channels < 1 || audio_config->channels > 2) {
    return false;
  }
  
  return true;
}

CodecError OpusEncoder::SetConfig(const AudioEncoderConfig& config) {
  if (config.type != CodecType::kOpus) {
    return CodecError::kInvalidParam;
  }
  config_ = config;
  return UpdateEncoderSettings();
}

std::unique_ptr<AudioEncoderConfig> OpusEncoder::GetAudioConfig() const {
  return std::make_unique<AudioEncoderConfig>(config_);
}

CodecError OpusEncoder::Encode(std::shared_ptr<RawFrame> input,
                               std::shared_ptr<EncodedFrame>* output) {
  if (!input || !output) {
    return CodecError::kInvalidParam;
  }
  
  if (state_ != CodecState::kRunning) {
    return CodecError::kNotInitialized;
  }
  
#ifdef MINIRTC_USE_OPUS
  if (!encoder_) {
    return CodecError::kNotInitialized;
  }
  
  const AudioFrameInfo& info = input->GetAudioInfo();
  const uint8_t* input_data = input->GetData();
  size_t input_size = input->GetSize();
  
  if (input_size == 0 || !input_data) {
    return CodecError::kInvalidParam;
  }
  
  // Calculate frame size
  int frame_size = static_cast<int>(config_.sample_rate * config_.frame_size_ms / 1000);
  
  // Prepare output buffer
  output_buffer_.resize(4000);  // Max opus packet size
  
  // Encode
  int encoded_size = opus_encode(encoder_, 
                                  reinterpret_cast<const opus_int16*>(input_data),
                                  frame_size,
                                  output_buffer_.data(),
                                  static_cast<opus_int32>(output_buffer_.size()));
  
  if (encoded_size < 0) {
    return CodecError::kStreamError;
  }
  
  // Create output frame
  auto encoded = std::make_shared<EncodedFrameImpl>(MediaType::kAudio);
  encoded->SetData(output_buffer_.data(), encoded_size);
  encoded->SetTimestampUs(input->GetTimestampUs());
  encoded->SetFrameNumber(++frame_counter_);
  encoded->SetKeyframe(true);  // Opus doesn't have keyframes, always mark as keyframe
  
  // Update stats
  stats_.encoded_frames++;
  stats_.encoded_bytes += encoded_size;
  
  *output = encoded;
  return CodecError::kOk;
  
#else
  // Stub implementation
  auto encoded = std::make_shared<EncodedFrameImpl>(MediaType::kAudio);
  encoded->SetTimestampUs(input->GetTimestampUs());
  encoded->SetFrameNumber(++frame_counter_);
  encoded->SetKeyframe(true);
  
  *output = encoded;
  return CodecError::kOk;
#endif
}

CodecError OpusEncoder::EncodeBatch(const std::vector<std::shared_ptr<RawFrame>>& inputs,
                                   std::vector<std::shared_ptr<EncodedFrame>>* outputs) {
  if (!outputs) {
    return CodecError::kInvalidParam;
  }
  
  outputs->clear();
  outputs->reserve(inputs.size());
  
  for (const auto& input : inputs) {
    std::shared_ptr<EncodedFrame> output;
    auto err = Encode(input, &output);
    if (err != CodecError::kOk) {
      return err;
    }
    outputs->push_back(output);
  }
  
  return CodecError::kOk;
}

CodecError OpusEncoder::Flush(std::vector<std::shared_ptr<EncodedFrame>>* outputs) {
  // No pending frames in Opus
  if (outputs) {
    outputs->clear();
  }
  return CodecError::kOk;
}

void OpusEncoder::RequestKeyframe() {
  // Audio has no keyframes - no-op for Opus
}

CodecError OpusEncoder::SetBitrate(uint32_t target_bps, uint32_t max_bps) {
  (void)max_bps;
  config_.bitrate_bps = target_bps;
  
#ifdef MINIRTC_USE_OPUS
  if (encoder_) {
    opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(target_bps));
  }
#endif
  
  return CodecError::kOk;
}

CodecError OpusEncoder::SetFramerate(uint32_t fps) {
  // Frame size is determined by frame_size_ms, not fps directly
  // Calculate frame_size_ms from fps
  if (fps > 0) {
    config_.frame_size_ms = 1000 / fps;
  }
  return UpdateEncoderSettings();
}

CodecError OpusEncoder::SetQuality(EncodeQuality quality) {
  config_.quality = quality;
  
  // Map quality to bitrate
  switch (quality) {
    case EncodeQuality::kLow:
      config_.bitrate_bps = 16000;
      break;
    case EncodeQuality::kMedium:
      config_.bitrate_bps = 32000;
      break;
    case EncodeQuality::kHigh:
      config_.bitrate_bps = 64000;
      break;
    case EncodeQuality::kUltra:
      config_.bitrate_bps = 128000;
      break;
  }
  
  return UpdateEncoderSettings();
}

void OpusEncoder::SetCallback(ICodecCallback* callback) {
  callback_ = callback;
}

void OpusEncoder::SetApplication(opus_int32 application) {
#ifdef MINIRTC_USE_OPUS
  if (encoder_) {
    opus_encoder_ctl(encoder_, OPUS_SET_APPLICATION(application));
  }
#endif
  (void)application;
}

void OpusEncoder::SetBandwidth(opus_int32 bandwidth) {
#ifdef MINIRTC_USE_OPUS
  if (encoder_) {
    opus_encoder_ctl(encoder_, OPUS_SET_BANDWIDTH(bandwidth));
  }
#endif
  (void)bandwidth;
}

void OpusEncoder::SetForceChannelMapping(bool force) {
  config_.force_channel_mapping = force;
#ifdef MINIRTC_USE_OPUS
  if (encoder_) {
    opus_encoder_ctl(encoder_, OPUS_SET_FORCE_CHANNELS(force ? config_.channels : OPUS_AUTO));
  }
#endif
}

CodecError OpusEncoder::CreateEncoder() {
#ifdef MINIRTC_USE_OPUS
  if (encoder_) {
    opus_encoder_destroy(encoder_);
    encoder_ = nullptr;
  }
  
  int error;
  encoder_ = opus_encoder_create(config_.sample_rate, 
                                 config_.channels,
                                 config_.application_kbe ? OPUS_APPLICATION_AUDIO : OPUS_APPLICATION_VOIP,
                                 &error);
  
  if (error != OPUS_OK || !encoder_) {
    state_ = CodecState::kError;
    return CodecError::kOutOfMemory;
  }
  
  state_ = CodecState::kInitialized;
  return UpdateEncoderSettings();
  
#else
  // Stub implementation
  encoder_ = reinterpret_cast<OpusEncoder*>(0x1);  // Mark as valid
  state_ = CodecState::kInitialized;
  return CodecError::kOk;
#endif
}

CodecError OpusEncoder::DestroyEncoder() {
#ifdef MINIRTC_USE_OPUS
  if (encoder_) {
    opus_encoder_destroy(encoder_);
    encoder_ = nullptr;
  }
#endif
  state_ = CodecState::kUninitialized;
  return CodecError::kOk;
}

CodecError OpusEncoder::UpdateEncoderSettings() {
#ifdef MINIRTC_USE_OPUS
  if (!encoder_) {
    return CodecError::kNotInitialized;
  }
  
  opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(config_.bitrate_bps));
  opus_encoder_ctl(encoder_, OPUS_SET_COMPLEXITY(config_.complexity));
  opus_encoder_ctl(encoder_, OPUS_SET_VBR(config_.vbr ? 1 : 0));
  opus_encoder_ctl(encoder_, OPUS_SET_VBR_CONSTRAINT(config_.vbr_constraint ? 1 : 0));
  
  // Calculate frame size
  frame_size_ = static_cast<int>(config_.sample_rate * config_.frame_size_ms / 1000);
  
  // Get lookahead
  opus_encoder_ctl(encoder_, OPUS_GET_LOOKAHEAD(&lookahead_));
  
  state_ = CodecState::kRunning;
#endif
  return CodecError::kOk;
}

// ===== Factory Function =====

std::unique_ptr<IEncoder> CreateOpusEncoder() {
  return std::make_unique<OpusEncoder>();
}

}  // namespace minirtc
