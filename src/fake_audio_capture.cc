/**
 * @file fake_audio_capture.cc
 * @brief Fake audio capture implementation
 */

#include "minirtc/fake_audio_capture.h"

#include <chrono>
#include <cstring>
#include <cmath>
#include <random>

namespace minirtc {
namespace fake {

using namespace std::chrono;

// ============================================================================
// AudioFrameGenerator Implementations
// ============================================================================

SineWaveGenerator::SineWaveGenerator(int sample_rate, int frequency_hz, int channels)
    : sample_rate_(sample_rate), frequency_hz_(frequency_hz), channels_(channels) {}

void SineWaveGenerator::GenerateFrame(AudioFrame* frame) {
    frame->sample_rate = sample_rate_;
    frame->channels = channels_;
    frame->format = AudioSampleFormat::kInt16;
    frame->channel_layout = channels_ == 1 ? AudioChannelLayout::kMono : AudioChannelLayout::kStereo;
    frame->samples_per_channel = 480;
    frame->AllocateBuffer();

    double phase_increment = 2.0 * M_PI * frequency_hz_ / sample_rate_;
    int16_t* samples = reinterpret_cast<int16_t*>(frame->data.data());

    for (int i = 0; i < frame->samples_per_channel * channels_; ++i) {
        double sample = sin(phase_increment * sample_index_);
        sample_index_++;
        samples[i] = static_cast<int16_t>(sample * 16000);
    }
}

SilenceGenerator::SilenceGenerator(int sample_rate, int channels)
    : sample_rate_(sample_rate), channels_(channels) {}

void SilenceGenerator::GenerateFrame(AudioFrame* frame) {
    frame->sample_rate = sample_rate_;
    frame->channels = channels_;
    frame->format = AudioSampleFormat::kInt16;
    frame->channel_layout = channels_ == 1 ? AudioChannelLayout::kMono : AudioChannelLayout::kStereo;
    frame->samples_per_channel = 480;
    frame->AllocateBuffer();
}

WhiteNoiseGenerator::WhiteNoiseGenerator(int sample_rate, int channels)
    : sample_rate_(sample_rate), channels_(channels) {}

void WhiteNoiseGenerator::GenerateFrame(AudioFrame* frame) {
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int16_t> dist(-16000, 16000);

    frame->sample_rate = sample_rate_;
    frame->channels = channels_;
    frame->format = AudioSampleFormat::kInt16;
    frame->channel_layout = channels_ == 1 ? AudioChannelLayout::kMono : AudioChannelLayout::kStereo;
    frame->samples_per_channel = 480;
    frame->AllocateBuffer();

    int16_t* samples = reinterpret_cast<int16_t*>(frame->data.data());
    for (int i = 0; i < frame->samples_per_channel * channels_; ++i) {
        samples[i] = dist(gen);
    }
}

// ============================================================================
// FakeAudioCapture
// ============================================================================

FakeAudioCapture::FakeAudioCapture() {
    frame_generator_ = std::make_unique<SineWaveGenerator>(48000, 440, 1);
}

FakeAudioCapture::~FakeAudioCapture() {
    StopCapture();
    Release();
}

void FakeAudioCapture::SetFrameGenerator(std::unique_ptr<AudioFrameGenerator> generator) {
    frame_generator_ = std::move(generator);
}

void FakeAudioCapture::SetGenerateSineWave(int frequency_hz) {
    frame_generator_ = std::make_unique<SineWaveGenerator>(
        param_.sample_rate > 0 ? param_.sample_rate : 48000,
        frequency_hz,
        param_.channels > 0 ? param_.channels : 1);
}

void FakeAudioCapture::SetSilence() {
    frame_generator_ = std::make_unique<SilenceGenerator>(
        param_.sample_rate > 0 ? param_.sample_rate : 48000,
        param_.channels > 0 ? param_.channels : 1);
}

void FakeAudioCapture::SetWhiteNoise() {
    frame_generator_ = std::make_unique<WhiteNoiseGenerator>(
        param_.sample_rate > 0 ? param_.sample_rate : 48000,
        param_.channels > 0 ? param_.channels : 1);
}

void FakeAudioCapture::SetGenerateVolume(float volume_db) {
    (void)volume_db;
}

ErrorCode FakeAudioCapture::GetDevices(std::vector<AudioDeviceInfo>* devices) {
    if (!devices) return ErrorCode::kInvalidParam;

    AudioDeviceInfo info;
    info.device_id = "fake_audio_device_0";
    info.device_name = "Fake Audio Input";
    info.unique_id = "fake_audio_unique_0";
    info.is_default = true;
    info.is_input = true;
    info.sample_rates = 0xFFF;
    info.channel_counts = 0x3;
    devices->push_back(info);

    return ErrorCode::kOk;
}

ErrorCode FakeAudioCapture::SetDevice(const std::string& device_id) {
    device_id_ = device_id;
    return ErrorCode::kOk;
}

std::string FakeAudioCapture::GetCurrentDevice() const {
    return device_id_.empty() ? "fake_audio_device_0" : device_id_;
}

ErrorCode FakeAudioCapture::SetParam(const AudioCaptureParam& param) {
    if (!param.IsValid()) return ErrorCode::kInvalidParam;
    param_ = param;

    if (frame_generator_) {
        frame_generator_ = std::make_unique<SineWaveGenerator>(
            param_.sample_rate, 440, param_.channels);
    }

    return ErrorCode::kOk;
}

ErrorCode FakeAudioCapture::GetParam(AudioCaptureParam* param) const {
    if (!param) return ErrorCode::kInvalidParam;
    *param = param_;
    return ErrorCode::kOk;
}

ErrorCode FakeAudioCapture::Initialize() {
    if (state_.load() != CaptureState::kIdle) {
        return ErrorCode::kAlreadyStarted;
    }

    state_.store(CaptureState::kInitializing);
    std::this_thread::sleep_for(milliseconds(10));
    state_.store(CaptureState::kReady);

    return ErrorCode::kOk;
}

ErrorCode FakeAudioCapture::StartCapture(AudioCaptureObserver* observer) {
    if (state_.load() == CaptureState::kCapturing) {
        return ErrorCode::kAlreadyStarted;
    }

    if (state_.load() != CaptureState::kReady) {
        return ErrorCode::kNotInitialized;
    }

    observer_ = observer;
    capturing_.store(true);
    state_.store(CaptureState::kCapturing);

    start_time_us_ = duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();

    capture_thread_ = std::thread(&FakeAudioCapture::CaptureThreadLoop, this);

    return ErrorCode::kOk;
}

ErrorCode FakeAudioCapture::StopCapture() {
    if (state_.load() != CaptureState::kCapturing) {
        return ErrorCode::kNotStarted;
    }

    state_.store(CaptureState::kStopping);
    capturing_.store(false);

    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }

    state_.store(CaptureState::kReady);
    return ErrorCode::kOk;
}

ErrorCode FakeAudioCapture::Release() {
    if (state_.load() == CaptureState::kCapturing) {
        StopCapture();
    }

    state_.store(CaptureState::kIdle);
    observer_ = nullptr;
    return ErrorCode::kOk;
}

CaptureState FakeAudioCapture::GetState() const {
    return state_.load();
}

bool FakeAudioCapture::IsCapturing() const {
    return state_.load() == CaptureState::kCapturing;
}

ErrorCode FakeAudioCapture::GetStats(AudioCaptureStats* stats) const {
    if (!stats) return ErrorCode::kInvalidParam;

    std::lock_guard<std::mutex> lock(stats_mutex_);
    *stats = stats_;
    return ErrorCode::kOk;
}

ErrorCode FakeAudioCapture::ResetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = AudioCaptureStats();
    seq_num_.store(0);
    timestamp_rtp_.store(0);
    return ErrorCode::kOk;
}

ErrorCode FakeAudioCapture::SetVolume(float volume) {
    volume_ = std::max(0.0f, std::min(1.0f, volume));
    return ErrorCode::kOk;
}

ErrorCode FakeAudioCapture::GetVolume(float* volume) const {
    if (!volume) return ErrorCode::kInvalidParam;
    *volume = volume_;
    return ErrorCode::kOk;
}

ErrorCode FakeAudioCapture::SetMute(bool mute) {
    mute_ = mute;
    return ErrorCode::kOk;
}

ErrorCode FakeAudioCapture::GetMute(bool* mute) const {
    if (!mute) return ErrorCode::kInvalidParam;
    *mute = mute_;
    return ErrorCode::kOk;
}

ErrorCode FakeAudioCapture::SetEchoReference(std::shared_ptr<AudioFrame> frame) {
    param_.echo_reference_ = frame;
    return ErrorCode::kOk;
}

void FakeAudioCapture::GenerateAndPushFrame() {
    if (!frame_generator_ || !observer_) return;

    AudioFrame frame;
    frame_generator_->GenerateFrame(&frame);

    if (!mute_ && volume_ < 1.0f) {
        int16_t* samples = reinterpret_cast<int16_t*>(frame.data.data());
        for (size_t i = 0; i < frame.data.size() / sizeof(int16_t); ++i) {
            samples[i] = static_cast<int16_t>(samples[i] * volume_);
        }
    }

    frame.timestamp_us = duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();
    frame.timestamp_rtp = timestamp_rtp_.fetch_add(param_.frames_per_buffer);
    frame.seq_num = seq_num_.fetch_add(1);

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_frames_captured++;
        stats_.total_bytes_captured += frame.GetDataSize();
    }

    observer_->OnFrameCaptured(frame);

    if (!mute_) {
        observer_->OnVolumeChanged(20.0f * log10f(volume_));
    }
}

void FakeAudioCapture::CaptureThreadLoop() {
    int64_t frame_interval_us = (int64_t)param_.frames_per_buffer * 1000000LL / param_.sample_rate;

    while (capturing_.load()) {
        auto frame_start = steady_clock::now();

        GenerateAndPushFrame();

        auto frame_end = steady_clock::now();
        auto elapsed = duration_cast<microseconds>(frame_end - frame_start).count();

        if (elapsed < frame_interval_us) {
            std::this_thread::sleep_for(microseconds(frame_interval_us - elapsed));
        }
    }
}

}  // namespace fake
}  // namespace minirtc
