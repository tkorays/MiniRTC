/**
 * @file h264_decoder.cpp
 * @brief H.264 decoder implementation
 */

#include "minirtc/codec/h264_decoder.h"
#include "minirtc/codec/encoder_frame.h"

namespace minirtc {

uint32_t H264Decoder::frame_counter_ = 0;

H264Decoder::H264Decoder() {
}

H264Decoder::~H264Decoder() {
  Release();
}

CodecError H264Decoder::Initialize(const ICodecConfig& config) {
  if (state_ == CodecState::kInitialized || state_ == CodecState::kRunning) {
    return CodecError::kAlreadyInitialized;
  }
  
  const VideoDecoderConfig* video_config = dynamic_cast<const VideoDecoderConfig*>(&config);
  if (!video_config) {
    return CodecError::kInvalidParam;
  }
  
  config_ = *video_config;
  return CreateDecoder();
}

CodecError H264Decoder::Release() {
  DestroyDecoder();
  state_ = CodecState::kStopped;
  return CodecError::kOk;
}

CodecError H264Decoder::Reset() {
  auto err = Release();
  if (err != CodecError::kOk) {
    return err;
  }
  return CreateDecoder();
}

std::unique_ptr<ICodecConfig> H264Decoder::GetConfig() const {
  return std::make_unique<VideoDecoderConfig>(config_);
}

CodecStats H264Decoder::GetStats() const {
  return stats_;
}

void H264Decoder::ResetStats() {
  stats_ = CodecStats();
}

bool H264Decoder::IsSupported(const ICodecConfig& config) const {
  const VideoDecoderConfig* video_config = dynamic_cast<const VideoDecoderConfig*>(&config);
  if (!video_config) {
    return false;
  }
  
  // Check if it's H.264
  if (video_config->type != CodecType::kH264) {
    return false;
  }
  
  return true;
}

CodecError H264Decoder::SetConfig(const VideoDecoderConfig& config) {
  if (config.type != CodecType::kH264) {
    return CodecError::kInvalidParam;
  }
  config_ = config;
  return CodecError::kOk;
}

std::unique_ptr<VideoDecoderConfig> H264Decoder::GetVideoConfig() const {
  return std::make_unique<VideoDecoderConfig>(config_);
}

CodecError H264Decoder::Decode(std::shared_ptr<EncodedFrame> input,
                               std::shared_ptr<RawFrame>* output) {
  if (!input || !output) {
    return CodecError::kInvalidParam;
  }
  
  if (state_ != CodecState::kRunning) {
    return CodecError::kNotInitialized;
  }
  
#ifdef MINIRTC_USE_H264
  if (!decoder_) {
    return CodecError::kNotInitialized;
  }
  
  const uint8_t* input_data = input->GetData();
  size_t input_size = input->GetSize();
  
  if (input_size == 0 || !input_data) {
    return CodecError::kInvalidParam;
  }
  
  // Prepare input buffer
  memset(&dec_output_info_, 0, sizeof(dec_output_info_));
  std::vector<uint8_t> input_buffer(input_size);
  memcpy(input_buffer.data(), input_data, input_size);
  
  // Decode frame
  SDecInputBuffer dec_input;
  memset(&dec_input, 0, sizeof(dec_input));
  dec_input.iBufferLen = static_cast<int>(input_size);
  dec_input.pData = input_buffer.data();
  
  int ret = decoder_->DecodeFrame2(&dec_input, &dec_output_info_);
  if (ret != 0) {
    return CodecError::kStreamError;
  }
  
  // Check if we got a decoded frame
  if (dec_output_info_.iBufferStatus == 1 && dec_output_info_.pDstY) {
    auto decoded = std::make_shared<RawFrameImpl>();
    VideoFrameInfo info;
    info.width = dec_output_info_.iWidth;
    info.height = dec_output_info_->iHeight;
    info.format = VideoPixelFormat::kI420;
    info.timestamp_us = input->GetTimestampUs();
    info.keyframe = input->IsKeyframe();
    info.stride[0] = dec_output_info_.iStride[0];
    info.stride[1] = dec_output_info_.iStride[1];
    info.stride[2] = dec_output_info_.iStride[2];
    
    decoded->SetVideoInfo(info);
    decoded->SetTimestampUs(input->GetTimestampUs());
    
    // Copy frame data
    size_t y_size = info.width * info.height;
    size_t uv_size = y_size / 4;
    std::vector<uint8_t> frame_data(y_size + uv_size * 2);
    
    // Copy Y plane
    memcpy(frame_data.data(), dec_output_info_.pDstY, y_size);
    // Copy U plane
    memcpy(frame_data.data() + y_size, dec_output_info_.pDstU, uv_size);
    // Copy V plane
    memcpy(frame_data.data() + y_size + uv_size, dec_output_info_.pDstV, uv_size);
    
    decoded->SetData(frame_data.data(), frame_data.size());
    
    stats_.decoded_frames++;
    stats_.decoded_bytes += input_size;
    
    *output = decoded;
    return CodecError::kOk;
  }
  
  return CodecError::kOk;  // Need more data
  
#endif  // MINIRTC_USE_H264

#ifdef MINIRTC_USE_FFMPEG
  if (!context_ || !packet_) {
    return CodecError::kNotInitialized;
  }
  
  const uint8_t* input_data = input->GetData();
  size_t input_size = input->GetSize();
  
  if (input_size == 0 || !input_data) {
    return CodecError::kInvalidParam;
  }
  
  // Set packet data
  av_packet_unref(packet_);
  packet_->data = const_cast<uint8_t*>(input_data);
  packet_->size = static_cast<int>(input_size);
  
  // Send packet to decoder
  int ret = avcodec_send_packet(context_, packet_);
  if (ret < 0) {
    return CodecError::kStreamError;
  }
  
  // Receive decoded frame
  ret = avcodec_receive_frame(context_, frame_);
  if (ret < 0) {
    if (ret == AVERROR(EAGAIN)) {
      return CodecError::kOk;  // Need more packets
    }
    return CodecError::kStreamError;
  }
  
  // Create output frame
  auto decoded = std::make_shared<RawFrameImpl>();
  VideoFrameInfo info;
  info.width = frame_->width;
  info.height = frame_->height;
  info.format = VideoPixelFormat::kI420;  // FFmpeg outputs I420
  info.timestamp_us = frame_->pts;
  info.keyframe = (frame_->pict_type == AV_PICTURE_TYPE_I);
  
  decoded->SetVideoInfo(info);
  decoded->SetTimestampUs(frame_->pts);
  
  // Copy frame data
  size_t y_size = frame_->width * frame_->height;
  size_t uv_size = y_size / 4;
  std::vector<uint8_t> frame_data(y_size + uv_size * 2);
  
  // Copy Y plane
  memcpy(frame_data.data(), frame_->data[0], y_size);
  // Copy U plane
  memcpy(frame_data.data() + y_size, frame_->data[1], uv_size);
  // Copy V plane
  memcpy(frame_data.data() + y_size + uv_size, frame_->data[2], uv_size);
  
  decoded->SetData(frame_data.data(), frame_data.size());
  
  // Update stats
  stats_.decoded_frames++;
  stats_.decoded_bytes += input_size;
  
  *output = decoded;
  return CodecError::kOk;
  
#else
  // Stub implementation
  auto decoded = std::make_shared<RawFrameImpl>();
  VideoFrameInfo info;
  info.width = config_.width;
  info.height = config_.height;
  info.format = config_.output_format;
  info.timestamp_us = input->GetTimestampUs();
  info.keyframe = input->IsKeyframe();
  decoded->SetVideoInfo(info);
  decoded->SetTimestampUs(input->GetTimestampUs());
  
  stats_.decoded_frames++;
  
  *output = decoded;
  return CodecError::kOk;
#endif
}

CodecError H264Decoder::DecodeBatch(const std::vector<std::shared_ptr<EncodedFrame>>& inputs,
                                    std::vector<std::shared_ptr<RawFrame>>* outputs) {
  if (!outputs) {
    return CodecError::kInvalidParam;
  }
  
  outputs->clear();
  outputs->reserve(inputs.size());
  
  for (const auto& input : inputs) {
    std::shared_ptr<RawFrame> output;
    auto err = Decode(input, &output);
    if (err != CodecError::kOk && err != CodecError::kBufferTooSmall) {
      return err;
    }
    if (output) {
      outputs->push_back(output);
    }
  }
  
  return CodecError::kOk;
}

CodecError H264Decoder::DecodeRaw(const uint8_t* data, size_t size,
                                  std::shared_ptr<RawFrame>* output) {
  if (!data || size == 0 || !output) {
    return CodecError::kInvalidParam;
  }
  
  // Create a temporary encoded frame and decode
  auto encoded = std::make_shared<EncodedFrameImpl>(MediaType::kVideo);
  encoded->SetData(data, size);
  
  return Decode(encoded, output);
}

CodecError H264Decoder::Flush(std::vector<std::shared_ptr<RawFrame>>* outputs) {
#ifdef MINIRTC_USE_FFMPEG
  if (!context_ || !outputs) {
    return CodecError::kInvalidParam;
  }
  
  outputs->clear();
  
  // Send null packet to flush decoder
  int ret = avcodec_send_packet(context_, nullptr);
  if (ret < 0) {
    return CodecError::kStreamError;
  }
  
  // Receive all pending frames
  while (ret >= 0) {
    ret = avcodec_receive_frame(context_, frame_);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    }
    if (ret < 0) {
      break;
    }
    
    // Create output frame (similar to Decode)
    auto decoded = std::make_shared<RawFrameImpl>();
    VideoFrameInfo info;
    info.width = frame_->width;
    info.height = frame_->height;
    decoded->SetVideoInfo(info);
    
    outputs->push_back(decoded);
  }
  
  return CodecError::kOk;
#else
  if (outputs) {
    outputs->clear();
  }
  return CodecError::kOk;
#endif
}

CodecError H264Decoder::SetParameterSets(const uint8_t* sps, size_t sps_size,
                                        const uint8_t* pps, size_t pps_size) {
  if (!sps || sps_size == 0) {
    return CodecError::kInvalidParam;
  }
  
  sps_.assign(sps, sps + sps_size);
  
  if (pps && pps_size > 0) {
    pps_.assign(pps, pps + pps_size);
  }
  
#ifdef MINIRTC_USE_H264
  if (decoder_) {
    // Set decoder parameter sets
    SDecParam dec_params;
    memset(&dec_params, 0, sizeof(dec_params));
    dec_params.pSpsBuffer = sps_.data();
    dec_params.uiSpsBufferLen = static_cast<unsigned int>(sps_size);
    dec_params.pPpsBuffer = pps_.data();
    dec_params.uiPpsBufferLen = static_cast<unsigned int>(pps_size);
    
    decoder_->SetOption(DECODER_OPTION_PARAM, &dec_params);
  }
#endif  // MINIRTC_USE_H264

#ifdef MINIRTC_USE_FFMPEG
  if (context_) {
    // Extract and set extradata
    std::vector<uint8_t> extradata;
    extradata.reserve(sps_size + pps_size + 4);
    
    // Add NAL start code
    extradata.push_back(0x00);
    extradata.push_back(0x00);
    extradata.push_back(0x00);
    extradata.push_back(0x01);
    extradata.insert(extradata.end(), sps, sps + sps_size);
    
    if (pps && pps_size > 0) {
      extradata.push_back(0x00);
      extradata.push_back(0x00);
      extradata.push_back(0x00);
      extradata.push_back(0x01);
      extradata.insert(extradata.end(), pps, pps + pps_size);
    }
    
    context_->extradata = static_cast<uint8_t*>(av_malloc(extradata.size()));
    memcpy(context_->extradata, extradata.data(), extradata.size());
    context_->extradata_size = static_cast<int>(extradata.size());
  }
#endif
  
  return CodecError::kOk;
}

std::vector<uint8_t> H264Decoder::GetSPS() const {
  return sps_;
}

std::vector<uint8_t> H264Decoder::GetPPS() const {
  return pps_;
}

bool H264Decoder::HasParameterSets() const {
  return !sps_.empty();
}

void H264Decoder::SetPacketLossRate(double loss_rate) {
  (void)loss_rate;
  // Could implement error concealment settings
}

void H264Decoder::NotifyPacketLost(uint16_t seq_start, uint16_t seq_end) {
  (void)seq_start;
  (void)seq_end;
  // Could implement PLC
}

void H264Decoder::SetCallback(ICodecCallback* callback) {
  callback_ = callback;
}

void H264Decoder::SetSEICallback(OnSEICallback callback) {
  sei_callback_ = callback;
}

uint32_t H264Decoder::GetDecoderFlags() const {
#ifdef MINIRTC_USE_FFMPEG
  if (context_) {
    return context_->flags;
  }
#endif
  return 0;
}

bool H264Decoder::NeedsDecoderReset() const {
  // Check if we need to reset decoder due to stream changes
  return false;
}

int H264Decoder::GetDecodingPictureCount() const {
#ifdef MINIRTC_USE_FFMPEG
  if (context_) {
    return context_->frame_num;
  }
#endif
#ifdef MINIRTC_USE_H264
  // OpenH264 doesn't expose frame count directly
  return 0;
#endif
  return 0;
}

CodecError H264Decoder::CreateDecoder() {
#ifdef MINIRTC_USE_H264
  if (decoder_) {
    decoder_->Uninitialize();
    decoder_ = nullptr;
  }
  
  // Create decoder
  int ret = WelsCreateSVCDecoder(&decoder_);
  if (ret != 0 || !decoder_) {
    state_ = CodecState::kError;
    return CodecError::kNotSupported;
  }
  
  // Set decoder parameters
  memset(&dec_params_, 0, sizeof(dec_params_));
  dec_params_.iPicWidth = config_.width;
  dec_params_.iPicHeight = config_.height;
  dec_params_.uiTargetDqLayers = 1;  // Decode all layers
  dec_params_.eEcMode = ERROR_CON_SLICE;  // Error concealment mode
  dec_params_.iOutputColorFormat = videoFormatI420;
  
  if (config_.low_latency) {
    dec_params_.bLowLatency = true;
  }
  
  // Initialize decoder
  ret = decoder_->Initialize(&dec_params_);
  if (ret != 0) {
    decoder_->Release();
    decoder_ = nullptr;
    state_ = CodecState::kError;
    return CodecError::kHardwareError;
  }
  
  is_valid_ = true;
  state_ = CodecState::kInitialized;
  return CodecError::kOk;
  
#endif  // MINIRTC_USE_H264

#ifdef MINIRTC_USE_FFMPEG
  if (context_) {
    avcodec_free_context(&context_);
    context_ = nullptr;
  }
  
  // Find H.264 decoder
  codec_ = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!codec_) {
    state_ = CodecState::kError;
    return CodecError::kNotSupported;
  }
  
  context_ = avcodec_alloc_context3(codec_);
  if (!context_) {
    state_ = CodecState::kError;
    return CodecError::kOutOfMemory;
  }
  
  // Set decoder parameters
  context_->thread_count = config_.thread_count > 0 ? config_.thread_count : 0;
  context_->flags2 |= AV_CODEC_FLAG2_FAST;
  
  if (config_.error_concealment) {
    context_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  }
  
  if (config_.low_latency) {
    context_->flags |= AV_CODEC_FLAG_LOW_DELAY;
  }
  
  // Open decoder
  int ret = avcodec_open2(context_, codec_, nullptr);
  if (ret < 0) {
    avcodec_free_context(&context_);
    state_ = CodecState::kError;
    return CodecError::kHardwareError;
  }
  
  // Allocate frame
  frame_ = av_frame_alloc();
  packet_ = av_packet_alloc();
  
  is_valid_ = true;
  state_ = CodecState::kInitialized;
  return CodecError::kOk;
  
#else
  // Stub implementation
  is_valid_ = true;
  state_ = CodecState::kInitialized;
  return CodecError::kOk;
#endif
}

CodecError H264Decoder::DestroyDecoder() {
#ifdef MINIRTC_USE_H264
  if (decoder_) {
    decoder_->Release();
    decoder_ = nullptr;
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
    if (context_->extradata) {
      av_free(context_->extradata);
      context_->extradata = nullptr;
    }
    avcodec_free_context(&context_);
    context_ = nullptr;
  }
#endif
#ifdef MINIRTC_USE_FFMPEG
  codec_ = nullptr;
#endif
  is_valid_ = false;
  sps_.clear();
  pps_.clear();
  state_ = CodecState::kUninitialized;
  return CodecError::kOk;
}

// ===== Factory Function =====

std::unique_ptr<IDecoder> CreateH264Decoder() {
  return std::make_unique<H264Decoder>();
}

}  // namespace minirtc
