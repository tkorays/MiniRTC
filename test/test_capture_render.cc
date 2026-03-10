/**
 * @file test_capture_render.cc
 * @brief Unit tests for capture and render modules
 */

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

#include "minirtc/capture_render.h"
#include "minirtc/fake_video_capture.h"
#include "minirtc/fake_audio_capture.h"
#include "minirtc/fake_video_renderer.h"
#include "minirtc/fake_audio_player.h"

using namespace minirtc;
using namespace minirtc::fake;

// ============================================================================
// Simple Test Framework
// ============================================================================

int g_tests_passed = 0;
int g_tests_failed = 0;

#define TEST_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            std::cerr << "FAIL: " << msg << " (" << #condition << ")" << std::endl; \
            return false; \
        } \
    } while(0)

#define TEST_EXPECT_EQ(a, b, msg) TEST_ASSERT((a) == (b), msg)
#define TEST_EXPECT_TRUE(a, msg) TEST_ASSERT((a), msg)
#define TEST_EXPECT_FALSE(a, msg) TEST_ASSERT(!(a), msg)
#define TEST_EXPECT_GT(a, b, msg) TEST_ASSERT((a) > (b), msg)

// ============================================================================
// TestVideoCaptureObserver
// ============================================================================

class TestVideoCaptureObserver : public VideoCaptureObserver {
public:
    void OnFrameCaptured(const VideoFrame& frame) override {
        frame_count_++;
        last_frame_ = frame;
    }

    void OnCaptureError(ErrorCode error_code, const std::string& error_msg) override {
        error_count_++;
        last_error_ = error_msg;
    }

    void OnDeviceChanged() override {
        device_changed_count_++;
    }

    int frame_count_ = 0;
    int error_count_ = 0;
    int device_changed_count_ = 0;
    VideoFrame last_frame_;
    std::string last_error_;
};

// ============================================================================
// TestAudioCaptureObserver
// ============================================================================

class TestAudioCaptureObserver : public AudioCaptureObserver {
public:
    void OnFrameCaptured(const AudioFrame& frame) override {
        frame_count_++;
        last_frame_ = frame;
    }

    void OnVolumeChanged(float volume_db) override {
        volume_change_count_++;
        last_volume_ = volume_db;
    }

    void OnMuteDetected(bool is_muted) override {
        mute_count_++;
        last_mute_ = is_muted;
    }

    void OnCaptureError(ErrorCode error_code, const std::string& error_msg) override {
        error_count_++;
    }

    void OnDeviceChanged() override {
        device_changed_count_++;
    }

    int frame_count_ = 0;
    int volume_change_count_ = 0;
    int mute_count_ = 0;
    int error_count_ = 0;
    int device_changed_count_ = 0;
    AudioFrame last_frame_;
    float last_volume_ = 0.0f;
    bool last_mute_ = false;
};

// ============================================================================
// TestVideoRenderObserver
// ============================================================================

class TestVideoRenderObserver : public VideoRenderObserver {
public:
    void OnFrameRendered(const VideoFrame& frame, int64_t render_time_us) override {
        render_count_++;
    }

    void OnRenderError(ErrorCode error_code, const std::string& error_msg) override {
        error_count_++;
    }

    void OnRenderStats(const VideoRenderStats& stats) override {
        stats_count_++;
    }

    int render_count_ = 0;
    int error_count_ = 0;
    int stats_count_ = 0;
};

// ============================================================================
// TestAudioPlayObserver
// ============================================================================

class TestAudioPlayObserver : public AudioPlayObserver {
public:
    void OnFramePlayed(const AudioFrame& frame, int64_t play_time_us) override {
        play_count_++;
    }

    void OnPlayError(ErrorCode error_code, const std::string& error_msg) override {
        error_count_++;
    }

    void OnPlayStats(const AudioPlayStats& stats) override {
        stats_count_++;
    }

    void OnBufferingChanged(bool is_buffering) override {
        buffering_count_++;
    }

    void OnMuteChanged(bool is_muted) override {
        mute_count_++;
    }

    int play_count_ = 0;
    int error_count_ = 0;
    int stats_count_ = 0;
    int buffering_count_ = 0;
    int mute_count_ = 0;
};

// ============================================================================
// FakeVideoCapture Tests
// ============================================================================

bool test_video_capture_init_release() {
    auto capture = std::make_unique<FakeVideoCapture>();
    
    TEST_EXPECT_EQ(capture->GetState(), CaptureState::kIdle, "Initial state");
    
    auto result = capture->Initialize();
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "Initialize");
    TEST_EXPECT_EQ(capture->GetState(), CaptureState::kReady, "State after init");
    
    result = capture->Release();
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "Release");
    TEST_EXPECT_EQ(capture->GetState(), CaptureState::kIdle, "State after release");
    
    return true;
}

bool test_video_capture_get_devices() {
    auto capture = std::make_unique<FakeVideoCapture>();
    
    std::vector<VideoDeviceInfo> devices;
    auto result = capture->GetDevices(&devices);
    
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "GetDevices");
    TEST_EXPECT_EQ(devices.size(), 1, "Device count");
    TEST_EXPECT_EQ(devices[0].device_name, "Fake Video Camera", "Device name");
    
    return true;
}

bool test_video_capture_set_param() {
    auto capture = std::make_unique<FakeVideoCapture>();
    
    VideoCaptureParam param;
    param.width = 1920;
    param.height = 1080;
    param.target_fps = 60;
    
    auto result = capture->SetParam(param);
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "SetParam");
    
    VideoCaptureParam get_param;
    capture->GetParam(&get_param);
    TEST_EXPECT_EQ(get_param.width, 1920, "Get width");
    TEST_EXPECT_EQ(get_param.height, 1080, "Get height");
    TEST_EXPECT_EQ(get_param.target_fps, 60, "Get fps");
    
    return true;
}

bool test_video_capture_start_stop() {
    auto capture = std::make_unique<FakeVideoCapture>();
    capture->Initialize();
    
    TestVideoCaptureObserver observer;
    auto result = capture->StartCapture(&observer);
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "StartCapture");
    TEST_EXPECT_TRUE(capture->IsCapturing(), "IsCapturing");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    result = capture->StopCapture();
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "StopCapture");
    TEST_EXPECT_FALSE(capture->IsCapturing(), "IsCapturing after stop");
    
    TEST_EXPECT_GT(observer.frame_count_, 0, "Frame count");
    
    return true;
}

bool test_video_capture_stats() {
    auto capture = std::make_unique<FakeVideoCapture>();
    capture->Initialize();
    
    VideoCaptureStats stats;
    auto result = capture->GetStats(&stats);
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "GetStats");
    
    TestVideoCaptureObserver observer;
    capture->StartCapture(&observer);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    capture->StopCapture();
    
    capture->GetStats(&stats);
    TEST_EXPECT_GT(stats.total_frames_captured, 0, "Total frames");
    
    return true;
}

// ============================================================================
// FakeAudioCapture Tests
// ============================================================================

bool test_audio_capture_init_release() {
    auto capture = std::make_unique<FakeAudioCapture>();
    
    TEST_EXPECT_EQ(capture->GetState(), CaptureState::kIdle, "Initial state");
    
    auto result = capture->Initialize();
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "Initialize");
    TEST_EXPECT_EQ(capture->GetState(), CaptureState::kReady, "State after init");
    
    result = capture->Release();
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "Release");
    
    return true;
}

bool test_audio_capture_get_devices() {
    auto capture = std::make_unique<FakeAudioCapture>();
    
    std::vector<AudioDeviceInfo> devices;
    auto result = capture->GetDevices(&devices);
    
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "GetDevices");
    TEST_EXPECT_EQ(devices.size(), 1, "Device count");
    TEST_EXPECT_EQ(devices[0].device_name, "Fake Audio Input", "Device name");
    
    return true;
}

bool test_audio_capture_volume_mute() {
    auto capture = std::make_unique<FakeAudioCapture>();
    capture->Initialize();
    
    auto result = capture->SetVolume(0.5f);
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "SetVolume");
    
    float volume = 0.0f;
    result = capture->GetVolume(&volume);
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "GetVolume");
    TEST_EXPECT_EQ(volume, 0.5f, "Volume value");
    
    result = capture->SetMute(true);
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "SetMute");
    
    bool mute = false;
    result = capture->GetMute(&mute);
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "GetMute");
    TEST_EXPECT_TRUE(mute, "Mute value");
    
    return true;
}

bool test_audio_capture_start_stop() {
    auto capture = std::make_unique<FakeAudioCapture>();
    capture->Initialize();
    
    TestAudioCaptureObserver observer;
    auto result = capture->StartCapture(&observer);
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "StartCapture");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    result = capture->StopCapture();
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "StopCapture");
    
    TEST_EXPECT_GT(observer.frame_count_, 0, "Frame count");
    
    return true;
}

// ============================================================================
// FakeVideoRenderer Tests
// ============================================================================

bool test_video_renderer_init_release() {
    auto renderer = std::make_unique<FakeVideoRenderer>();
    
    TEST_EXPECT_EQ(renderer->GetState(), PlayState::kIdle, "Initial state");
    
    VideoRenderParam param;
    param.output_width = 1280;
    param.output_height = 720;
    
    auto result = renderer->Initialize(param);
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "Initialize");
    TEST_EXPECT_EQ(renderer->GetState(), PlayState::kReady, "State after init");
    
    result = renderer->Release();
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "Release");
    
    return true;
}

bool test_video_renderer_render_frame() {
    auto renderer = std::make_unique<FakeVideoRenderer>();
    
    VideoRenderParam param;
    param.output_width = 640;
    param.output_height = 480;
    renderer->Initialize(param);
    
    TestVideoRenderObserver observer;
    renderer->StartRender(&observer);
    
    VideoFrame frame(640, 480, VideoPixelFormat::kRGBA);
    frame.timestamp_us = 1234567890;
    
    auto result = renderer->RenderFrame(frame);
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "RenderFrame");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    renderer->StopRender();
    
    TEST_EXPECT_GT(observer.render_count_, 0, "Render count");
    
    return true;
}

bool test_video_renderer_display_control() {
    auto renderer = std::make_unique<FakeVideoRenderer>();
    
    VideoRenderParam param;
    param.output_width = 1280;
    param.output_height = 720;
    renderer->Initialize(param);
    
    TEST_EXPECT_EQ(renderer->SetVisible(false), ErrorCode::kOk, "SetVisible");
    TEST_EXPECT_EQ(renderer->SetBackground(0xFF000000), ErrorCode::kOk, "SetBackground");
    TEST_EXPECT_EQ(renderer->SetRotation(90), ErrorCode::kOk, "SetRotation");
    TEST_EXPECT_EQ(renderer->SetMirror(true), ErrorCode::kOk, "SetMirror");
    TEST_EXPECT_EQ(renderer->SetAlpha(0.5f), ErrorCode::kOk, "SetAlpha");
    TEST_EXPECT_EQ(renderer->SetDisplayRegion(0, 0, 640, 480), ErrorCode::kOk, "SetDisplayRegion");
    
    return true;
}

// ============================================================================
// FakeAudioPlayer Tests
// ============================================================================

bool test_audio_player_init_release() {
    auto player = std::make_unique<FakeAudioPlayer>();
    
    TEST_EXPECT_EQ(player->GetState(), PlayState::kIdle, "Initial state");
    
    auto result = player->Initialize();
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "Initialize");
    TEST_EXPECT_EQ(player->GetState(), PlayState::kReady, "State after init");
    
    result = player->Release();
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "Release");
    
    return true;
}

bool test_audio_player_get_devices() {
    auto player = std::make_unique<FakeAudioPlayer>();
    
    std::vector<AudioDeviceInfo> devices;
    auto result = player->GetDevices(&devices);
    
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "GetDevices");
    TEST_EXPECT_EQ(devices.size(), 1, "Device count");
    TEST_EXPECT_EQ(devices[0].device_name, "Fake Audio Output", "Device name");
    
    return true;
}

bool test_audio_player_volume_mute() {
    auto player = std::make_unique<FakeAudioPlayer>();
    player->Initialize();
    
    TEST_EXPECT_EQ(player->SetVolume(0.7f), ErrorCode::kOk, "SetVolume");
    
    float volume = 0.0f;
    TEST_EXPECT_EQ(player->GetVolume(&volume), ErrorCode::kOk, "GetVolume");
    TEST_EXPECT_EQ(volume, 0.7f, "Volume value");
    
    TEST_EXPECT_EQ(player->SetMute(true), ErrorCode::kOk, "SetMute");
    
    bool mute = true;
    TEST_EXPECT_EQ(player->GetMute(&mute), ErrorCode::kOk, "GetMute");
    TEST_EXPECT_TRUE(mute, "Mute value");
    
    return true;
}

bool test_audio_player_playback_rate() {
    auto player = std::make_unique<FakeAudioPlayer>();
    player->Initialize();
    
    TEST_EXPECT_EQ(player->SetPlaybackRate(1.5f), ErrorCode::kOk, "SetPlaybackRate");
    
    float rate = 0.0f;
    TEST_EXPECT_EQ(player->GetPlaybackRate(&rate), ErrorCode::kOk, "GetPlaybackRate");
    TEST_EXPECT_EQ(rate, 1.5f, "Rate value");
    
    return true;
}

bool test_audio_player_play_frame() {
    auto player = std::make_unique<FakeAudioPlayer>();
    player->Initialize();
    
    TestAudioPlayObserver observer;
    player->StartPlay(&observer);
    
    AudioFrame frame(48000, 1, 480);
    frame.timestamp_us = 1234567890;
    
    auto result = player->PlayFrame(frame);
    TEST_EXPECT_EQ(result, ErrorCode::kOk, "PlayFrame");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    player->StopPlay();
    
    TEST_EXPECT_GT(observer.play_count_, 0, "Play count");
    
    return true;
}

// ============================================================================
// Factory Tests
// ============================================================================

bool test_factory_create() {
    auto capture = CaptureFactory::CreateVideoCapture(CaptureFactory::CaptureType::kFake);
    TEST_EXPECT_TRUE(capture != nullptr, "CreateVideoCapture");
    
    auto audio_cap = CaptureFactory::CreateAudioCapture(CaptureFactory::CaptureType::kFake);
    TEST_EXPECT_TRUE(audio_cap != nullptr, "CreateAudioCapture");
    
    auto renderer = RendererFactory::CreateVideoRenderer(RendererFactory::RendererType::kFake);
    TEST_EXPECT_TRUE(renderer != nullptr, "CreateVideoRenderer");
    
    auto player = RendererFactory::CreateAudioPlayer(RendererFactory::RendererType::kFake);
    TEST_EXPECT_TRUE(player != nullptr, "CreateAudioPlayer");
    
    return true;
}

bool test_irect_create() {
    auto capture = IVideoCapture::Create();
    TEST_EXPECT_TRUE(capture != nullptr, "IVideoCapture::Create");
    
    auto audio_cap = IAudioCapture::Create();
    TEST_EXPECT_TRUE(audio_cap != nullptr, "IAudioCapture::Create");
    
    auto renderer = IVideoRenderer::Create();
    TEST_EXPECT_TRUE(renderer != nullptr, "IVideoRenderer::Create");
    
    auto player = IAudioPlayer::Create();
    TEST_EXPECT_TRUE(player != nullptr, "IAudioPlayer::Create");
    
    return true;
}

// ============================================================================
// Test Runner
// ============================================================================

#define RUN_TEST(name) \
    do { \
        std::cout << "Running " << #name << "..." << std::flush; \
        if (name()) { \
            std::cout << " PASSED" << std::endl; \
            g_tests_passed++; \
        } else { \
            std::cout << " FAILED" << std::endl; \
            g_tests_failed++; \
        } \
    } while(0)

void run_capture_render_tests() {
    std::cout << "=== MiniRTC Capture/Render Tests ===" << std::endl;
    
    // VideoCapture tests
    RUN_TEST(test_video_capture_init_release);
    RUN_TEST(test_video_capture_get_devices);
    RUN_TEST(test_video_capture_set_param);
    RUN_TEST(test_video_capture_start_stop);
    RUN_TEST(test_video_capture_stats);
    
    // AudioCapture tests
    RUN_TEST(test_audio_capture_init_release);
    RUN_TEST(test_audio_capture_get_devices);
    RUN_TEST(test_audio_capture_volume_mute);
    RUN_TEST(test_audio_capture_start_stop);
    
    // VideoRenderer tests
    RUN_TEST(test_video_renderer_init_release);
    RUN_TEST(test_video_renderer_render_frame);
    RUN_TEST(test_video_renderer_display_control);
    
    // AudioPlayer tests
    RUN_TEST(test_audio_player_init_release);
    RUN_TEST(test_audio_player_get_devices);
    RUN_TEST(test_audio_player_volume_mute);
    RUN_TEST(test_audio_player_playback_rate);
    RUN_TEST(test_audio_player_play_frame);
    
    // Factory tests
    RUN_TEST(test_factory_create);
    RUN_TEST(test_irect_create);
    
    std::cout << "======================================" << std::endl;
    std::cout << "Capture/Render: " << g_tests_passed << " passed, " << g_tests_failed << " failed" << std::endl;
}

// External declaration for codec tests
// extern int run_codec_tests();

int main() {
    // Run capture/render tests
    // g_tests_passed = 0;
    // g_tests_failed = 0;
    
    run_capture_render_tests();
    
    int result = g_tests_failed;
    
    std::cout << "\n=== FINAL RESULT: " << (result == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << " ===" << std::endl;
    
    return result;
}
