/**
 * @file h264_decoder.h
 * @brief MiniRTC H.264 video decoder
 */

#ifndef MINIRTC_H264_DECODER_H_
#define MINIRTC_H264_DECODER_H_

#include "idecoder.h"
#include "decoder_config.h"
#include <functional>

#ifdef MINIRTC_USE_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#endif

namespace minirtc {

/**
 * @brief H.264 video decoder
 * 
 * Based on FFmpeg/libavcodec implementation
 */
class H264Decoder : public IDecoder {
 public:
  H264Decoder();
  ~H264Decoder() override;
  
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
  
  // ===== IDecoder Interface =====
  
  CodecError SetConfig(const VideoDecoderConfig& config) override;
  std::unique_ptr<VideoDecoderConfig> GetVideoConfig() const override;
  
  CodecError Decode(std::shared_ptr<EncodedFrame> input,
                   std::shared_ptr<RawFrame>* output) override;
  CodecError DecodeBatch(const std::vector<std::shared_ptr<EncodedFrame>>& inputs,
                        std::vector<std::shared_ptr<RawFrame>>* outputs) override;
  CodecError DecodeRaw(const uint8_t* data, size_t size,
                      std::shared_ptr<RawFrame>* output) override;
  CodecError Flush(std::vector<std::shared_ptr<RawFrame>>* outputs) override;
  
  CodecError SetParameterSets(const uint8_t* sps, size_t sps_size,
                             const uint8_t* pps, size_t pps_size) override;
  std::vector<uint8_t> GetSPS() const override;
  std::vector<uint8_t> GetPPS() const override;
  bool HasParameterSets() const override;
  
  void SetPacketLossRate(double loss_rate) override;
  void NotifyPacketLost(uint16_t seq_start, uint16_t seq_end) override;
  
  void SetCallback(ICodecCallback* callback) override;
  
  // ===== H.264 Specific Interfaces =====
  
  /// Set SEI message callback
  using OnSEICallback = std::function<void(const uint8_t* data, size_t size)>;
  void SetSEICallback(OnSEICallback callback);
  
  /// Get decoder flags
  uint32_t GetDecoderFlags() const;
  
  /// Check if decoder needs reset
  bool NeedsDecoderReset() const;
  
  /// Get current decoding picture count
  int GetDecodingPictureCount() const;
  
  /// Check if decoder is valid
  bool IsValid() const { return is_valid_; }

 private:
  CodecError CreateDecoder();
  CodecError DestroyDecoder();
  
#ifdef MINIRTC_USE_FFMPEG
  const AVCodec* codec_ = nullptr;
  AVCodecContext* context_ = nullptr;
  AVFrame* frame_ = nullptr;
  AVPacket* packet_ = nullptr;
  SwsContext* sws_context_ = nullptr;
#else
  void* codec_context_ = nullptr;
#endif
  
  VideoDecoderConfig config_;
  CodecState state_ = CodecState::kUninitialized;
  CodecStats stats_;
  ICodecCallback* callback_ = nullptr;
  
  bool is_valid_ = false;
  
  // SPS/PPS cache
  std::vector<uint8_t> sps_;
  std::vector<uint8_t> pps_;
  
  // SEI callback
  OnSEICallback sei_callback_;
  
  // Static counter for frame numbers
  static uint32_t frame_counter_;
};

/**
 * @brief Create H.264 decoder
 */
std::unique_ptr<IDecoder> CreateH264Decoder();

}  // namespace minirtc

#endif  // MINIRTC_H264_DECODER_H_
