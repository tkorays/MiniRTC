/**
 * @file capture_render_factory.cc
 * @brief Factory implementations for capture and render modules
 */

#include "minirtc/capture_render.h"
#include "minirtc/fake_video_capture.h"
#include "minirtc/fake_audio_capture.h"
#include "minirtc/fake_video_renderer.h"
#include "minirtc/fake_audio_player.h"

#include <unordered_map>

namespace minirtc {

// ============================================================================
// IVideoCapture::Create()
// ============================================================================

std::unique_ptr<IVideoCapture> IVideoCapture::Create() {
    return CaptureFactory::CreateVideoCapture(CaptureFactory::CaptureType::kFake);
}

// ============================================================================
// IAudioCapture::Create()
// ============================================================================

std::unique_ptr<IAudioCapture> IAudioCapture::Create() {
    return CaptureFactory::CreateAudioCapture(CaptureFactory::CaptureType::kFake);
}

// ============================================================================
// IVideoRenderer::Create()
// ============================================================================

std::unique_ptr<IVideoRenderer> IVideoRenderer::Create() {
    return RendererFactory::CreateVideoRenderer(RendererFactory::RendererType::kFake);
}

// ============================================================================
// IAudioPlayer::Create()
// ============================================================================

std::unique_ptr<IAudioPlayer> IAudioPlayer::Create() {
    return RendererFactory::CreateAudioPlayer(RendererFactory::RendererType::kFake);
}

// ============================================================================
// CaptureFactory
// ============================================================================

namespace {

using VideoCaptureCreatorMap = std::unordered_map<CaptureFactory::CaptureType, CaptureFactory::VideoCaptureCreator>;
using AudioCaptureCreatorMap = std::unordered_map<CaptureFactory::CaptureType, CaptureFactory::AudioCaptureCreator>;

VideoCaptureCreatorMap& GetVideoCaptureCreators() {
    static VideoCaptureCreatorMap creators;
    return creators;
}

AudioCaptureCreatorMap& GetAudioCaptureCreators() {
    static AudioCaptureCreatorMap creators;
    return creators;
}

bool g_video_capture_factory_initialized = false;
bool g_audio_capture_factory_initialized = false;

void InitializeVideoCaptureFactory() {
    if (g_video_capture_factory_initialized) return;
    
    auto& creators = GetVideoCaptureCreators();
    creators[CaptureFactory::CaptureType::kFake] = []() {
        return std::make_unique<fake::FakeVideoCapture>();
    };
    
    g_video_capture_factory_initialized = true;
}

void InitializeAudioCaptureFactory() {
    if (g_audio_capture_factory_initialized) return;
    
    auto& creators = GetAudioCaptureCreators();
    creators[CaptureFactory::CaptureType::kFake] = []() {
        return std::make_unique<fake::FakeAudioCapture>();
    };
    
    g_audio_capture_factory_initialized = true;
}

}  // anonymous namespace

std::unique_ptr<IVideoCapture> CaptureFactory::CreateVideoCapture(CaptureType type) {
    InitializeVideoCaptureFactory();
    
    auto& creators = GetVideoCaptureCreators();
    auto it = creators.find(type);
    
    if (it != creators.end()) {
        return it->second();
    }
    
    return std::make_unique<fake::FakeVideoCapture>();
}

std::unique_ptr<IAudioCapture> CaptureFactory::CreateAudioCapture(CaptureType type) {
    InitializeAudioCaptureFactory();
    
    auto& creators = GetAudioCaptureCreators();
    auto it = creators.find(type);
    
    if (it != creators.end()) {
        return it->second();
    }
    
    return std::make_unique<fake::FakeAudioCapture>();
}

void CaptureFactory::RegisterVideoCapture(CaptureType type, VideoCaptureCreator creator) {
    InitializeVideoCaptureFactory();
    
    auto& creators = GetVideoCaptureCreators();
    creators[type] = std::move(creator);
}

void CaptureFactory::RegisterAudioCapture(CaptureType type, AudioCaptureCreator creator) {
    InitializeAudioCaptureFactory();
    
    auto& creators = GetAudioCaptureCreators();
    creators[type] = std::move(creator);
}

// ============================================================================
// RendererFactory
// ============================================================================

namespace {

using VideoRendererCreatorMap = std::unordered_map<RendererFactory::RendererType, RendererFactory::VideoRendererCreator>;
using AudioPlayerCreatorMap = std::unordered_map<RendererFactory::RendererType, RendererFactory::AudioPlayerCreator>;

VideoRendererCreatorMap& GetVideoRendererCreators() {
    static VideoRendererCreatorMap creators;
    return creators;
}

AudioPlayerCreatorMap& GetAudioPlayerCreators() {
    static AudioPlayerCreatorMap creators;
    return creators;
}

bool g_video_renderer_factory_initialized = false;
bool g_audio_player_factory_initialized = false;

void InitializeVideoRendererFactory() {
    if (g_video_renderer_factory_initialized) return;
    
    auto& creators = GetVideoRendererCreators();
    creators[RendererFactory::RendererType::kFake] = []() {
        return std::make_unique<fake::FakeVideoRenderer>();
    };
    
    g_video_renderer_factory_initialized = true;
}

void InitializeAudioPlayerFactory() {
    if (g_audio_player_factory_initialized) return;
    
    auto& creators = GetAudioPlayerCreators();
    creators[RendererFactory::RendererType::kFake] = []() {
        return std::make_unique<fake::FakeAudioPlayer>();
    };
    
    g_audio_player_factory_initialized = true;
}

}  // anonymous namespace

std::unique_ptr<IVideoRenderer> RendererFactory::CreateVideoRenderer(RendererType type) {
    InitializeVideoRendererFactory();
    
    auto& creators = GetVideoRendererCreators();
    auto it = creators.find(type);
    
    if (it != creators.end()) {
        return it->second();
    }
    
    return std::make_unique<fake::FakeVideoRenderer>();
}

std::unique_ptr<IAudioPlayer> RendererFactory::CreateAudioPlayer(RendererType type) {
    InitializeAudioPlayerFactory();
    
    auto& creators = GetAudioPlayerCreators();
    auto it = creators.find(type);
    
    if (it != creators.end()) {
        return it->second();
    }
    
    return std::make_unique<fake::FakeAudioPlayer>();
}

void RendererFactory::RegisterVideoRenderer(RendererType type, VideoRendererCreator creator) {
    InitializeVideoRendererFactory();
    
    auto& creators = GetVideoRendererCreators();
    creators[type] = std::move(creator);
}

void RendererFactory::RegisterAudioPlayer(RendererType type, AudioPlayerCreator creator) {
    InitializeAudioPlayerFactory();
    
    auto& creators = GetAudioPlayerCreators();
    creators[type] = std::move(creator);
}

}  // namespace minirtc
