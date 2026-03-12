/**
 * @file h264_encoder.cpp
 * @brief H.264 encoder implementation
 */

#include "minirtc/codec/h264_encoder.h"
#include "minirtc/codec/encoder_frame.h"

namespace minirtc {

uint32_t H264Encoder::frame_counter_ = 0;

H264Encoder::H264Encoder() {
}

H264Encoder::~H264Encoder() {
  Release();
}

CodecError H264Encoder::Initialize(const ICodecConfig& config) {
  if (state_ == CodecState::kInitialized || state_ == CodecState::kRunning) {
    return CodecError::kAlreadyInitialized;
  }
  
  const VideoEncoderConfig* video_config = dynamic_cast<const VideoEncoderConfig*>(&config);
  if (!video_config) {
    return CodecError::kInvalidParam;
  }
  
  config_ = *video_config;
  return CreateEncoder();
}

CodecError H264Encoder::Release() {
  DestroyEncoder();
  state_ = CodecState::kStopped;
  return CodecError::kOk;
}

CodecError H264Encoder::Reset() {
  auto err = Release();
  if (err != CodecError::kOk) {
    return err;
  }
  return CreateEncoder();
}

std::unique_ptr<ICodecConfig> H264Encoder::GetConfig() const {
  return std::make_unique<VideoEncoderConfig>(config_);
}

CodecStats H264Encoder::GetStats() const {
  return stats_;
}

void H264Encoder::ResetStats() {
  stats_ = CodecStats();
}

bool H264Encoder::IsSupported(const ICodecConfig& config) const {
  const VideoEncoderConfig* video_config = dynamic_cast<const VideoEncoderConfig*>(&config);
  if (!video_config) {
    return false;
  }
  
  // Check if it's H.264
  if (video_config->type != CodecType::kH264) {
    return false;
  }
  
  // Basic validation
  if (video_config->width == 0 || video_config->height == 0) {
    return false;
  }
  
  return true;
}

CodecError H264Encoder::SetConfig(const VideoEncoderConfig& config) {
  if (config.type != CodecType::kH264) {
    return CodecError::kInvalidParam;
  }
  config_ = config;
  return UpdateEncoderSettings();
}

std::unique_ptr<VideoEncoderConfig> H264Encoder::GetVideoConfig() const {
  return std::make_unique<VideoEncoderConfig>(config_);
}

CodecError H264Encoder::Encode(std::shared_ptr<RawFrame> input,
                                std::shared_ptr<EncodedFrame>* output) {
  if (!input || !output) {
    return CodecError::kInvalidParam;
  }
  
  if (state_ != CodecState::kRunning) {
    return CodecError::kNotInitialized;
  }
  
#ifdef MINIRTC_USE_H264
  if (!encoder_) {
    return CodecError::kNotInitialized;
  }
  
  const VideoFrameInfo& info = input->GetVideoInfo();
  
  // Prepare source picture
  memset(&src_pic_, 0, sizeof(src_pic_));
  src_pic_.iPicWidth = info.width;
  src_pic_.iPicHeight = info.height;
  src_pic_.iColorFormat = videoFormatI420;
  
  // Copy Y plane
  const uint8_t* y_data = input->GetPlaneData(0);
  if (y_data) {
    src_pic_.pData[0] = const_cast<uint8_t*>(y_data);
    src_pic_.iStride[0] = info.stride[0];
  }
  
  // Copy U plane
  const uint8_t* u_data = input->GetPlaneData(1);
  if (u_data) {
    src_pic_.pData[1] = const_cast<uint8_t*>(u_data);
    src_pic_.iStride[1] = info.stride[1];
  }
  
  // Copy V plane
  const uint8_t* v_data = input->GetPlaneData(2);
  if (v_data) {
    src_pic_.pData[2] = const_cast<uint8_t*>(v_data);
    src_pic_.iStride[2] = info.stride[2];
  }
  
  // Encode frame
  memset(&enc_output_info_, 0, sizeof(enc_output_info_));
  int ret = encoder_->EncodeFrame(&src_pic_, &enc_output_info_);
  if (ret != 0) {
    return CodecError::kStreamError;
  }
  
  // Create output frame
  auto encoded = std::make_shared<EncodedFrameImpl>(MediaType::kVideo);
  encoded->SetTimestampUs(input->GetTimestampUs());
  encoded->SetFrameNumber(++frame_counter_);
  
  // Process NAL units
  for (int i = 0; i < enc_output_info_.iNalCount; i++) {
    encoded->AddNALUnit(enc_output_info_.pNalList[i].pBsBuf, 
                        enc_output_info_.pNalList[i].iNalSize);
    
    // Check if this is an IDR frame (keyframe)
    // NAL type 5 = IDR slice
    uint8_t nal_type = enc_output_info_.pNalList[i].pBsBuf[4] & 0x1F;
    if (nal_type == 5 || nal_type == 7) {
      encoded->SetKeyframe(true);
    }
  }
  
  // Update stats
  stats_.encoded_frames++;
  
  *output = encoded;
  return CodecError::kOk;
  
#endif  // MINIRTC_USE_H264

#ifdef MINIRTC_USE_FFMPEG
  if (!context_ || !frame_ || !packet_) {
    return CodecError::kNotInitialized;
  }
  
  const VideoFrameInfo& info = input->GetVideoInfo();
  
  // Set frame data
  frame_->width = info.width;
  frame_->height = info.height;
  frame_->pts = input->GetTimestampUs();
  
  // For I420 format
  const uint8_t* y_data = input->GetPlaneData(0);
  if (y_data) {
    // Copy Y plane
    for (int i = 0; i < info.height; i++) {
      memcpy(frame_->data[0] + i * frame_->linesize[0],
             y_data + i * info.stride[0],
             info.width);
    }
    // Copy U and V planes similarly...
  }
  
  // Send frame to encoder
  int ret = avcodec_send_frame(context_, frame_);
  if (ret < 0) {
    return CodecError::kStreamError;
  }
  
  // Receive encoded packet
  ret = avcodec_receive_packet(context_, packet_);
  if (ret < 0) {
    return CodecError::kStreamError;
  }
  
  // Create output frame
  auto encoded = std::make_shared<EncodedFrameImpl>(MediaType::kVideo);
  encoded->SetData(packet_->data, packet_->size);
  encoded->SetTimestampUs(input->GetTimestampUs());
  encoded->SetFrameNumber(++frame_counter_);
  encoded->SetKeyframe((packet_->flags & AV_PKT_FLAG_KEY) != 0);
  
  // Update stats
  stats_.encoded_frames++;
  stats_.encoded_bytes += packet_->size;
  
  av_packet_unref(packet_);
  
  *output = encoded;
  return CodecError::kOk;
  
#else
  // Stub implementation
  auto encoded = std::make_shared<EncodedFrameImpl>(MediaType::kVideo);
  encoded->SetTimestampUs(input->GetTimestampUs());
  encoded->SetFrameNumber(++frame_counter_);
  encoded->SetKeyframe(true);  // Always keyframe in stub
  
  // Add stub NAL unit
  const uint8_t nal_header[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1F};
  encoded->AddNALUnit(nal_header, sizeof(nal_header));
  
  stats_.encoded_frames++;
  
  *output = encoded;
  return CodecError::kOk;
#endif
}

CodecError H264Encoder::EncodeBatch(const std::vector<std::shared_ptr<RawFrame>>& inputs,
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

CodecError H264Encoder::Flush(std::vector<std::shared_ptr<EncodedFrame>>* outputs) {
#ifdef MINIRTC_USE_H264
  if (!encoder_ || !outputs) {
    return CodecError::kInvalidParam;
  }
  
  outputs->clear();
  
  // Flush encoder by sending NULL frame
  memset(&src_pic_, 0, sizeof(src_pic_));
  src_pic_.iPicWidth = config_.width;
  src_pic_.iPicHeight = config_.height;
  src_pic_.iColorFormat = videoFormatI420;
  
  memset(&enc_output_info_, 0, sizeof(enc_output_info_));
  int ret = encoder_->EncodeFrame(&src_pic_, &enc_output_info_);
  
  while (ret == 0 && enc_output_info_.iNalCount > 0) {
    auto encoded = std::make_shared<EncodedFrameImpl>(MediaType::kVideo);
    
    for (int i = 0; i < enc_output_info_.iNalCount; i++) {
      encoded->AddNALUnit(enc_output_info_.pNalList[i].pBsBuf,
                          enc_output_info_.pNalList[i].iNalSize);
    }
    
    encoded->SetFrameNumber(++frame_counter_);
    outputs->push_back(encoded);
    
    memset(&enc_output_info_, 0, sizeof(enc_output_info_));
    ret = encoder_->EncodeFrame(nullptr, &enc_output_info_);
  }
  
  return CodecError::kOk;

#endif  // MINIRTC_USE_H264

#ifdef MINIRTC_USE_FFMPEG
  if (!context_ || !outputs) {
    return CodecError::kInvalidParam;
  }
  
  outputs->clear();
  
  // Send null frame to flush encoder
  int ret = avcodec_send_frame(context_, nullptr);
  if (ret < 0) {
    return CodecError::kStreamError;
  }
  
  // Receive all pending packets
  while (ret >= 0) {
    ret = avcodec_receive_packet(context_, packet_);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    }
    if (ret < 0) {
      return CodecError::kStreamError;
    }
    
    auto encoded = std::make_shared<EncodedFrameImpl>(MediaType::kVideo);
    encoded->SetData(packet_->data, packet_->size);
    encoded->SetFrameNumber(++frame_counter_);
    encoded->SetKeyframe((packet_->flags & AV_PKT_FLAG_KEY) != 0);
    
    outputs->push_back(encoded);
    av_packet_unref(packet_);
  }
  
  return CodecError::kOk;
#else
  if (outputs) {
    outputs->clear();
  }
  return CodecError::kOk;
#endif
}

void H264Encoder::RequestKeyframe() {
  if (idr_request_enabled_) {
    // Force IDR frame
#ifdef MINIRTC_USE_H264
    if (encoder_) {
      encoder_->EncodeParameterSets(true);
    }
#endif

#ifdef MINIRTC_USE_FFMPEG
    // FFmpeg-specific reinit if needed
#endif
  }
}

CodecError H264Encoder::SetBitrate(uint32_t target_kbps, uint32_t max_kbps) {
  config_.target_bitrate_kbps = target_kbps;
  config_.max_bitrate_kbps = max_kbps;
  
#ifdef MINIRTC_USE_H264
  if (encoder_) {
    SEncParamExt params;
    memset(&params, 0, sizeof(params));
    params.iUsageType = CAMERA_VIDEO_REAL_TIME;
    params.iTargetBitrate = target_kbps * 1000;
    params.iMaxBitrate = max_kbps * 1000;
    encoder_->SetOption(ENCODER_OPTION_PARAM, &params);
  }
#endif

#ifdef MINIRTC_USE_FFMPEG
  if (context_) {
    context_->bit_rate = target_kbps * 1000;
    context_->rc_buffer_size = max_kbps * 1000 * 2;
    context_->rc_max_rate = max_kbps * 1000;
  }
#endif
  
  return CodecError::kOk;
}

CodecError H264Encoder::SetFramerate(uint32_t fps) {
  config_.framerate = fps;
  
#ifdef MINIRTC_USE_H264
  if (encoder_) {
    SEncParamExt params;
    memset(&params, 0, sizeof(params));
    params.iUsageType = CAMERA_VIDEO_REAL_TIME;
    params.fMaxFrameRate = static_cast<float>(fps);
    encoder_->SetOption(ENCODER_OPTION_PARAM, &params);
  }
#endif

#ifdef MINIRTC_USE_FFMPEG
  if (context_) {
    context_->time_base = {1, static_cast<int>(fps)};
  }
#endif
  
  return UpdateEncoderSettings();
}

CodecError H264Encoder::SetQuality(EncodeQuality quality) {
  config_.quality = quality;
  
  // Map quality to bitrate
  switch (quality) {
    case EncodeQuality::kLow:
      config_.target_bitrate_kbps = 500;
      config_.max_bitrate_kbps = 800;
      break;
    case EncodeQuality::kMedium:
      config_.target_bitrate_kbps = 1000;
      config_.max_bitrate_kbps = 1500;
      break;
    case EncodeQuality::kHigh:
      config_.target_bitrate_kbps = 2000;
      config_.max_bitrate_kbps = 3000;
      break;
    case EncodeQuality::kUltra:
      config_.target_bitrate_kbps = 4000;
      config_.max_bitrate_kbps = 6000;
      break;
  }
  
  return SetBitrate(config_.target_bitrate_kbps, config_.max_bitrate_kbps);
}

void H264Encoder::SetCallback(ICodecCallback* callback) {
  callback_ = callback;
}

std::vector<uint8_t> H264Encoder::GetSPS() const {
  return sps_;
}

std::vector<uint8_t> H264Encoder::GetPPS() const {
  return pps_;
}

void H264Encoder::GetSupportedResolutions(std::vector<std::pair<uint32_t, uint32_t>>* resolutions) const {
  if (!resolutions) return;
  
  // Common resolutions
  resolutions->push_back({640, 480});    // VGA
  resolutions->push_back({1280, 720});   // HD
  resolutions->push_back({1920, 1080});  // Full HD
  resolutions->push_back({2560, 1440});  // QHD
  resolutions->push_back({3840, 2160});  // 4K
}

uint32_t H264Encoder::GetMaxFramerate() const {
  return 60;
}

uint32_t H264Encoder::GetMaxBitrate() const {
  return 20000;  // 20 Mbps
}

bool H264Encoder::IsHardwareAccelerationAvailable() const {
#ifdef MINIRTC_USE_H264
  // OpenH264 is software encoder, no HW acceleration
  return false;
#endif

#ifdef MINIRTC_USE_FFMPEG
  // Check for hardware encoder
  return false;  // Simplified
#else
  return false;
#endif
}

CodecError H264Encoder::CreateEncoder() {
#ifdef MINIRTC_USE_H264
  if (encoder_) {
    encoder_->Uninitialize();
    encoder_ = nullptr;
  }
  
  // Create encoder
  int ret = WelsCreateSVCEncoder(&encoder_);
  if (ret != 0 || !encoder_) {
    state_ = CodecState::kError;
    return CodecError::kNotSupported;
  }
  
  // Set encoder parameters
  memset(&enc_params_, 0, sizeof(enc_params_));
  enc_params_.iUsageType = CAMERA_VIDEO_REAL_TIME;
  enc_params_.fMaxFrameRate = static_cast<float>(config_.framerate);
  enc_params_.iTargetBitrate = config_.target_bitrate_kbps * 1000;
  enc_params_.iMaxBitrate = config_.max_bitrate_kbps * 1000;
  enc_params_.iPicWidth = config_.width;
  enc_params_.iPicHeight = config_.height;
  enc_params_.iRCMode = RC_BITRATE_MODE;
  
  // Profile
  if (config_.profile == "baseline") {
    enc_params_.eProfile = PRO_BASELINE;
  } else if (config_.profile == "main") {
    enc_params_.eProfile = PRO_MAIN;
  } else {
    enc_params_.eProfile = PRO_HIGH;
  }
  
  // Entropy mode
  if (config_.entropy_mode == "cavlc") {
    enc_params_.iEntropyMode = 0;  // CAVLC
  } else {
    enc_params_.iEntropyMode = 1;  // CABAC
  }
  
  // Set encoder param
  ret = encoder_->Initialize(&enc_params_);
  if (ret != 0) {
    encoder_->Release();
    encoder_ = nullptr;
    state_ = CodecState::kError;
    return CodecError::kHardwareError;
  }
  
  // Request SPS/PPS
  encoder_->EncodeParameterSets(true);
  
  is_valid_ = true;
  state_ = CodecState::kInitialized;
  return UpdateEncoderSettings();
  
#endif  // MINIRTC_USE_H264

#ifdef MINIRTC_USE_FFMPEG
  if (context_) {
    avcodec_free_context(&context_);
    context_ = nullptr;
  }
  
  // Find H.264 encoder
  codec_ = avcodec_find_encoder(AV_CODEC_ID_H264);
  if (!codec_) {
    state_ = CodecState::kError;
    return CodecError::kNotSupported;
  }
  
  context_ = avcodec_alloc_context3(codec_);
  if (!context_) {
    state_ = CodecState::kError;
    return CodecError::kOutOfMemory;
  }
  
  // Set encoder parameters
  context_->width = config_.width;
  context_->height = config_.height;
  context_->time_base = {1, static_cast<int>(config_.framerate)};
  context_->bit_rate = config_.target_bitrate_kbps * 1000;
  context_->rc_buffer_size = config_.max_bitrate_kbps * 1000 * 2;
  context_->rc_max_rate = config_.max_bitrate_kbps * 1000;
  
  // GOP settings
  context_->gop_size = config_.keyframe_interval;
  context_->max_b_frames = config_.max_bframes;
  
  // Pixel format - required for H.264
  context_->pix_fmt = AV_PIX_FMT_YUV420P;
  
  // Profile
  if (config_.profile == "baseline") {
    context_->profile = AV_PROFILE_H264_BASELINE;
  } else if (config_.profile == "main") {
    context_->profile = AV_PROFILE_H264_MAIN;
  } else {
    context_->profile = AV_PROFILE_H264_HIGH;
  }
  
  // Entropy mode - removed in newer FFmpeg, use profile-based default
  // Cavlc is default for baseline, cabac for main/high
  
  // Threading
  if (config_.thread_count > 0) {
    context_->thread_count = config_.thread_count;
  }
  
  // Open encoder
  int ret = avcodec_open2(context_, codec_, nullptr);
  if (ret < 0) {
    avcodec_free_context(&context_);
    state_ = CodecState::kError;
    return CodecError::kHardwareError;
  }
  
  // Allocate frame and packet
  frame_ = av_frame_alloc();
  packet_ = av_packet_alloc();
  
  frame_->format = AV_PIX_FMT_YUV420P;
  frame_->width = config_.width;
  frame_->height = config_.height;
  av_frame_get_buffer(frame_, 32);
  
  is_valid_ = true;
  state_ = CodecState::kInitialized;
  return UpdateEncoderSettings();
  
#else
  // Stub implementation
  is_valid_ = true;
  state_ = CodecState::kInitialized;
  return CodecError::kOk;
#endif
}

CodecError H264Encoder::DestroyEncoder() {
#ifdef MINIRTC_USE_H264
  if (encoder_) {
    encoder_->Release();
    encoder_ = nullptr;
  }
#endif  // MINIRTC_USE_H264

#ifdef MINIRTC_USE_FFMPEG
  if (frame_) {
    av_frame_free(&frame_);
    frame_ = nullptr;
  }
  if (packet_) {
    av_packet_free(&packet_);
    packet_ = nullptr;
  }
  if (context_) {
    avcodec_free_context(&context_);
    context_ = nullptr;
  }
#endif
#ifdef MINIRTC_USE_FFMPEG
  codec_ = nullptr;
#endif
  is_valid_ = false;
  state_ = CodecState::kUninitialized;
  return CodecError::kOk;
}

CodecError H264Encoder::UpdateEncoderSettings() {
#ifdef MINIRTC_USE_H264
  if (!encoder_) {
    return CodecError::kNotInitialized;
  }
  
  // Update bitrate settings
  SEncParamExt params;
  memset(&params, 0, sizeof(params));
  encoder_->GetOption(ENCODER_OPTION_PARAM, &params);
  
  params.iTargetBitrate = config_.target_bitrate_kbps * 1000;
  params.iMaxBitrate = config_.max_bitrate_kbps * 1000;
  params.fMaxFrameRate = static_cast<float>(config_.framerate);
  
  encoder_->SetOption(ENCODER_OPTION_PARAM, &params);
  
#endif  // MINIRTC_USE_H264

#ifdef MINIRTC_USE_FFMPEG
  if (!context_) {
    return CodecError::kNotInitialized;
  }
  
  // Update any runtime-changeable settings
  context_->bit_rate = config_.target_bitrate_kbps * 1000;
#endif
  state_ = CodecState::kRunning;
  return CodecError::kOk;
}

// ===== Factory Function =====

std::unique_ptr<IEncoder> CreateH264Encoder() {
  return std::make_unique<H264Encoder>();
}

}  // namespace minirtc
