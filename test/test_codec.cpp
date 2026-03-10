/**
 * @file test_codec.cpp
 * @brief Codec module unit tests (simple version)
 */

#include <iostream>
#include <cassert>
#include <vector>
#include "minirtc/codec/codec_types.h"
#include "minirtc/codec/icodec.h"
#include "minirtc/codec/encoder_frame.h"
#include "minirtc/codec/encoder_config.h"
#include "minirtc/codec/decoder_config.h"
#include "minirtc/codec/iencoder.h"
#include "minirtc/codec/idecoder.h"
#include "minirtc/codec/opus_encoder.h"
#include "minirtc/codec/opus_decoder.h"
#include "minirtc/codec/h264_encoder.h"
#include "minirtc/codec/h264_decoder.h"
#include "minirtc/codec/codec_factory.h"

using namespace minirtc;

int test_codec_types() {
    std::cout << "Testing codec types..." << std::endl;
    
    // Test codec type enum
    assert(static_cast<int>(CodecType::kOpus) == 1);
    assert(static_cast<int>(CodecType::kH264) == 100);
    
    // Test media type enum
    assert(static_cast<int>(MediaType::kAudio) == 1);
    assert(static_cast<int>(MediaType::kVideo) == 2);
    
    // Test video frame info
    VideoFrameInfo vinfo;
    assert(vinfo.width == 0);
    assert(vinfo.height == 0);
    vinfo.width = 1280;
    vinfo.height = 720;
    assert(vinfo.width == 1280);
    
    // Test audio frame info
    AudioFrameInfo ainfo;
    assert(ainfo.sample_rate == 48000);
    assert(ainfo.channels == 2);
    ainfo.sample_rate = 16000;
    assert(ainfo.sample_rate == 16000);
    
    std::cout << "  PASSED" << std::endl;
    return 0;
}

int test_encoder_frame() {
    std::cout << "Testing encoder frames..." << std::endl;
    
    // Test raw frame
    auto raw_frame = std::make_shared<RawFrameImpl>();
    AudioFrameInfo ainfo;
    ainfo.sample_rate = 48000;
    ainfo.channels = 2;
    ainfo.samples_per_channel = 480;
    raw_frame->SetAudioInfo(ainfo);
    
    assert(raw_frame->GetMediaType() == MediaType::kAudio);
    assert(raw_frame->GetAudioInfo().sample_rate == 48000);
    
    // Test video frame
    auto video_frame = std::make_shared<RawFrameImpl>();
    VideoFrameInfo vinfo;
    vinfo.width = 1280;
    vinfo.height = 720;
    video_frame->SetVideoInfo(vinfo);
    
    assert(video_frame->GetMediaType() == MediaType::kVideo);
    assert(video_frame->GetVideoInfo().width == 1280);
    
    // Test encoded frame
    auto encoded = std::make_shared<EncodedFrameImpl>(MediaType::kVideo);
    assert(encoded->GetMediaType() == MediaType::kVideo);
    assert(!encoded->IsKeyframe());
    
    encoded->SetKeyframe(true);
    assert(encoded->IsKeyframe());
    
    encoded->SetTimestampUs(1000000);
    assert(encoded->GetTimestampUs() == 1000000);
    
    // Test NAL units
    const uint8_t nal_data[] = {0x00, 0x00, 0x00, 0x01, 0x67};
    encoded->AddNALUnit(nal_data, sizeof(nal_data));
    assert(!encoded->GetNALUnits().empty());
    
    // Test clone
    auto clone = encoded->Clone();
    assert(clone->IsKeyframe() == encoded->IsKeyframe());
    assert(clone->GetTimestampUs() == encoded->GetTimestampUs());
    
    std::cout << "  PASSED" << std::endl;
    return 0;
}

int test_encoder_config() {
    std::cout << "Testing encoder config..." << std::endl;
    
    // Video encoder config
    VideoEncoderConfig vconfig;
    assert(vconfig.type == CodecType::kH264);
    assert(vconfig.media_type == MediaType::kVideo);
    assert(vconfig.width == 1280);
    assert(vconfig.height == 720);
    assert(vconfig.framerate == 30);
    assert(vconfig.target_bitrate_kbps == 1000);
    
    vconfig.width = 1920;
    vconfig.height = 1080;
    assert(vconfig.width == 1920);
    assert(vconfig.height == 1080);
    
    // Test clone
    auto cloned = vconfig.Clone();
    assert(cloned->GetType() == CodecType::kH264);
    
    // Test ToString
    std::string json = vconfig.ToString();
    assert(!json.empty());
    
    // Audio encoder config
    AudioEncoderConfig aconfig;
    assert(aconfig.type == CodecType::kOpus);
    assert(aconfig.sample_rate == 48000);
    assert(aconfig.channels == 2);
    assert(aconfig.bitrate_bps == 64000);
    
    std::cout << "  PASSED" << std::endl;
    return 0;
}

int test_decoder_config() {
    std::cout << "Testing decoder config..." << std::endl;
    
    VideoDecoderConfig vconfig;
    assert(vconfig.type == CodecType::kH264);
    assert(vconfig.output_format == VideoPixelFormat::kI420);
    assert(vconfig.use_hardware == true);
    
    AudioDecoderConfig aconfig;
    assert(aconfig.type == CodecType::kOpus);
    assert(aconfig.output_format == AudioSampleFormat::kS16);
    assert(aconfig.packet_loss_concealment == true);
    
    std::cout << "  PASSED" << std::endl;
    return 0;
}

int test_opus_encoder() {
    std::cout << "Testing Opus encoder..." << std::endl;
    
    auto encoder = std::make_unique<minirtc::OpusEncoder>();
    
    assert(encoder->GetType() == CodecType::kOpus);
    assert(encoder->GetMediaType() == MediaType::kAudio);
    assert(encoder->GetState() == CodecState::kUninitialized);
    
    AudioEncoderConfig config;
    config.sample_rate = 48000;
    config.channels = 2;
    
    auto err = encoder->Initialize(config);
    assert(err == CodecError::kOk);
    assert(encoder->GetState() == CodecState::kInitialized);
    
    // Test GetConfig
    auto retrieved = encoder->GetAudioConfig();
    assert(retrieved != nullptr);
    assert(retrieved->sample_rate == 48000);
    assert(retrieved->channels == 2);
    
    // Test SetBitrate
    err = encoder->SetBitrate(32000, 48000);
    assert(err == CodecError::kOk);
    
    // Test SetQuality
    err = encoder->SetQuality(EncodeQuality::kHigh);
    assert(err == CodecError::kOk);
    
    encoder->Release();
    assert(encoder->GetState() == CodecState::kStopped);
    
    std::cout << "  PASSED" << std::endl;
    return 0;
}

int test_h264_encoder() {
    std::cout << "Testing H264 encoder..." << std::endl;
    
    auto encoder = std::make_unique<minirtc::H264Encoder>();
    
    assert(encoder->GetType() == CodecType::kH264);
    assert(encoder->GetMediaType() == MediaType::kVideo);
    
    VideoEncoderConfig config;
    config.width = 1280;
    config.height = 720;
    
    // Initialize (may fail without FFmpeg, but that's ok for stub)
    auto err = encoder->Initialize(config);
    
    // Test supported resolutions
    std::vector<std::pair<uint32_t, uint32_t>> resolutions;
    encoder->GetSupportedResolutions(&resolutions);
    assert(!resolutions.empty());
    
    // Test max bitrate
    uint32_t max_bitrate = encoder->GetMaxBitrate();
    assert(max_bitrate > 0);
    
    encoder->Release();
    
    std::cout << "  PASSED" << std::endl;
    return 0;
}

int test_codec_factory() {
    std::cout << "Testing codec factory..." << std::endl;
    
    auto& factory = CodecFactory::Instance();
    
    // Test create Opus encoder
    AudioEncoderConfig audio_config;
    auto opus_enc = factory.CreateEncoder(audio_config);
    assert(opus_enc != nullptr);
    assert(opus_enc->GetType() == CodecType::kOpus);
    
    // Test create Opus decoder
    AudioDecoderConfig audio_dec_config;
    auto opus_dec = factory.CreateDecoder(audio_dec_config);
    assert(opus_dec != nullptr);
    assert(opus_dec->GetType() == CodecType::kOpus);
    
    // Test create H264 encoder (may fail without FFmpeg)
    VideoEncoderConfig video_config;
    video_config.width = 1280;
    video_config.height = 720;
    auto h264_enc = factory.CreateEncoder(video_config);
    // May be nullptr without FFmpeg
    
    // Test create H264 decoder
    VideoDecoderConfig video_dec_config;
    auto h264_dec = factory.CreateDecoder(video_dec_config);
    // May be nullptr without FFmpeg
    
    // Test GetSupportedEncoders
    auto audio_encoders = factory.GetSupportedEncoders(MediaType::kAudio);
    assert(!audio_encoders.empty());
    assert(std::find(audio_encoders.begin(), audio_encoders.end(), CodecType::kOpus) 
           != audio_encoders.end());
    
    // Test GetSupportedDecoders
    auto audio_decoders = factory.GetSupportedDecoders(MediaType::kAudio);
    assert(!audio_decoders.empty());
    
    std::cout << "  PASSED" << std::endl;
    return 0;
}

int test_icodec_static() {
    std::cout << "Testing ICodec static methods..." << std::endl;
    
    // Test GetCodecName
    assert(ICodec::GetCodecName(CodecType::kOpus) == "Opus");
    assert(ICodec::GetCodecName(CodecType::kH264) == "H.264/AVC");
    assert(ICodec::GetCodecName(CodecType::kVP8) == "VP8");
    assert(ICodec::GetCodecName(static_cast<CodecType>(999)) == "Unknown");
    
    // Test GetSupportedCodecs
    auto codecs = ICodec::GetSupportedCodecs(MediaType::kAudio);
    assert(!codecs.empty());
    
    std::cout << "  PASSED" << std::endl;
    return 0;
}

int run_codec_tests() {
    std::cout << "=== MiniRTC Codec Module Tests ===" << std::endl;
    
    int failed = 0;
    
    failed += test_codec_types();
    failed += test_encoder_frame();
    failed += test_encoder_config();
    failed += test_decoder_config();
    failed += test_opus_encoder();
    failed += test_h264_encoder();
    failed += test_codec_factory();
    failed += test_icodec_static();
    
    if (failed == 0) {
        std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
        return 0;
    } else {
        std::cout << "\n=== " << failed << " TESTS FAILED ===" << std::endl;
        return 1;
    }
}
