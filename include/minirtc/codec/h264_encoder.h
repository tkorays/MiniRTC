/**
 * @file h264_encoder.h
 * @brief MiniRTC H.264 video encoder
 */

#ifndef MINIRTC_H264_ENCODER_H_
#define MINIRTC_H264_ENCODER_H_

#include "iencoder.h"
#include "encoder_config.h"

#ifdef MINIRTC_USE_H264
#include <wels/codec_api.h>
#include <wels/codec_def.h>
#endif

#ifdef MINIRTC_USE_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#endif

namespace minirtc {

/**
 * @brief H.264 video encoder
 * 
 * Based on FFmpeg/libx264 implementation
 */
class H264Encoder : public IEncoder {
 public:
  H264Encoder();
  ~H264Encoder() override;
  
  // ===== ICodec Interface =====
  
  CodecError Initialize(const ICodecConfig& config) override;
  CodecError Release() override;
  CodecError Reset() override;
  
  CodecType GetType() const override { return CodecType::kH264; }
  MediaType GetMediaType() const override { return MediaType::kVideo; }
  CodecState GetState() const override { return state_; }
  std::unique_ptr<ICodecConfig> GetConfig() const override;
  
  CodecStats GetStats() const override;
  void ResetStats() override;
  
  bool IsSupported(const ICodecConfig& config) const override;
  
  // ===== IEncoder Interface =====
  
  CodecError SetConfig(const VideoEncoderConfig& config) override;
  std::unique_ptr<VideoEncoderConfig> GetVideoConfig() const override;
  
  CodecError Encode(std::shared_ptr<RawFrame> input,
                   std::shared_ptr<EncodedFrame>* output) override;
  CodecError EncodeBatch(const std::vector<std::shared_ptr<RawFrame>>& inputs,
                        std::vector<std::shared_ptr<EncodedFrame>>* outputs) override;
  CodecError Flush(std::vector<std::shared_ptr<EncodedFrame>>* outputs) override;
  
  void RequestKeyframe() override;
  CodecError SetBitrate(uint32_t target_kbps, uint32_t max_kbps) override;
  CodecError SetFramerate(uint32_t fps) override;
  CodecError SetQuality(EncodeQuality quality) override;
  
  void SetCallback(ICodecCallback* callback) override;
  
  // ===== H.264 Specific Interfaces =====
  
  /// Get SPS
  std::vector<uint8_t> GetSPS() const;
  
  /// Get PPS
  std::vector<uint8_t> GetPPS() const;
  
  /// Get VPS (HEVC)
  std::vector<uint8_t> GetVPS() const { return {}; }
  
  /// Enable/Disable Instantaneous Refresh Decoding Frame (IDR)
  void SetIDRRequestEnabled(bool enabled) { idr_request_enabled_ = enabled; }
  
  /// Check if encoder is valid
  bool IsValid() const { return is_valid_; }
  
  // ===== Capability Query =====
  
  void GetSupportedResolutions(std::vector<std::pair<uint32_t, uint32_t>>* resolutions) const override;
  uint32_t GetMaxFramerate() const override;
  uint32_t GetMaxBitrate() const override;
  bool IsHardwareAccelerationAvailable() const override;

 private:
  CodecError CreateEncoder();
  CodecError DestroyEncoder();
  CodecError UpdateEncoderSettings();
  
#ifdef MINIRTC_USE_H264
  ISVCEncoder* encoder_ = nullptr;
  SEncParamBase enc_params_;
  SFrameBSInfo enc_output_info_;
  SSourcePicture src_pic_;
#endif

#ifdef MINIRTC_USE_FFMPEG
  const AVCodec* codec_ = nullptr;
  AVCodecContext* context_ = nullptr;
  AVFrame* frame_ = nullptr;
  AVPacket* packet_ = nullptr;
#else
  void* codec_context_ = nullptr;
#endif
  
  VideoEncoderConfig config_;
  CodecState state_ = CodecState::kUninitialized;
  CodecStats stats_;
  ICodecCallback* callback_ = nullptr;
  
  bool is_valid_ = false;
  bool idr_request_enabled_ = true;
  
  // SPS/PPS
  std::vector<uint8_t> sps_;
  std::vector<uint8_t> pps_;
  
  // Static counter for frame numbers
  static uint32_t frame_counter_;
};

/**
 * @brief Create H.264 encoder
 */
std::unique_ptr<IEncoder> CreateH264Encoder();

}  // namespace minirtc

#endif  // MINIRTC_H264_ENCODER_H_
