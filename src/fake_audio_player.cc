/**
 * @file fake_audio_player.cc
 * @brief Fake audio player implementation
 */

#include "minirtc/fake_audio_player.h"

#include <chrono>
#include <random>
#include <cmath>

namespace minirtc {
namespace fake {

using namespace std::chrono;

// ============================================================================
// FakeAudioPlayer
// ============================================================================

FakeAudioPlayer::FakeAudioPlayer() {}

FakeAudioPlayer::~FakeAudioPlayer() {
    StopPlay();
    Release();
}

void FakeAudioPlayer::SetPlayLatency(int64_t latency_us) {
    play_latency_us_ = latency_us;
}

void FakeAudioPlayer::SetDropFrameRate(float rate) {
    drop_frame_rate_ = std::max(0.0f, std::min(1.0f, rate));
}

void FakeAudioPlayer::SetProcessFrames(bool process) {
    process_frames_ = process;
}

ErrorCode FakeAudioPlayer::GetDevices(std::vector<AudioDeviceInfo>* devices) {
    if (!devices) return ErrorCode::kInvalidParam;

    AudioDeviceInfo info;
    info.device_id = "fake_audio_player_0";
    info.device_name = "Fake Audio Output";
    info.unique_id = "fake_audio_player_unique_0";
    info.is_default = true;
    info.is_input = false;
    info.sample_rates = 0xFFF;
    info.channel_counts = 0x3;
    devices->push_back(info);

    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::SetDevice(const std::string& device_id) {
    device_id_ = device_id;
    return ErrorCode::kOk;
}

std::string FakeAudioPlayer::GetCurrentDevice() const {
    return device_id_.empty() ? "fake_audio_player_0" : device_id_;
}

ErrorCode FakeAudioPlayer::SetParam(const AudioPlayParam& param) {
    if (!param.IsValid()) return ErrorCode::kInvalidParam;
    param_ = param;
    volume_ = param.volume;
    mute_ = param.mute;
    playback_rate_ = param.playback_rate;
    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::GetParam(AudioPlayParam* param) const {
    if (!param) return ErrorCode::kInvalidParam;
    *param = param_;
    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::Initialize() {
    if (state_.load() != PlayState::kIdle) {
        return ErrorCode::kAlreadyStarted;
    }

    state_.store(PlayState::kInitializing);
    std::this_thread::sleep_for(milliseconds(10));
    state_.store(PlayState::kReady);

    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::StartPlay(AudioPlayObserver* observer) {
    if (state_.load() == PlayState::kPlaying) {
        return ErrorCode::kAlreadyStarted;
    }

    if (state_.load() != PlayState::kReady) {
        return ErrorCode::kNotInitialized;
    }

    observer_ = observer;
    playing_.store(true);
    state_.store(PlayState::kPlaying);

    stats_.play_start_time_us = duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();

    play_thread_ = std::thread(&FakeAudioPlayer::PlayThreadLoop, this);

    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::StopPlay() {
    if (state_.load() != PlayState::kPlaying) {
        return ErrorCode::kNotStarted;
    }

    state_.store(PlayState::kStopping);
    playing_.store(false);
    frame_cv_.notify_all();

    if (play_thread_.joinable()) {
        play_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(frame_queue_mutex_);
        while (!frame_queue_.empty()) {
            frame_queue_.pop();
        }
    }

    state_.store(PlayState::kReady);
    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::Release() {
    if (state_.load() == PlayState::kPlaying) {
        StopPlay();
    }

    state_.store(PlayState::kIdle);
    observer_ = nullptr;
    return ErrorCode::kOk;
}

PlayState FakeAudioPlayer::GetState() const {
    return state_.load();
}

bool FakeAudioPlayer::IsPlaying() const {
    return state_.load() == PlayState::kPlaying;
}

bool FakeAudioPlayer::IsBuffering() const {
    return buffering_;
}

ErrorCode FakeAudioPlayer::PlayFrame(const AudioFrame& frame) {
    if (state_.load() != PlayState::kPlaying) {
        return ErrorCode::kNotStarted;
    }

    ProcessFrame(frame);
    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::PlayFrameList(const std::vector<AudioFrame>& frames) {
    if (state_.load() != PlayState::kPlaying) {
        return ErrorCode::kNotStarted;
    }

    for (const auto& frame : frames) {
        ProcessFrame(frame);
    }
    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::SetVolume(float volume) {
    volume_ = std::max(0.0f, std::min(1.0f, volume));
    param_.volume = volume_;
    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::GetVolume(float* volume) const {
    if (!volume) return ErrorCode::kInvalidParam;
    *volume = volume_;
    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::SetMute(bool mute) {
    bool was_muted = mute_;
    mute_ = mute;
    param_.mute = mute;
    if (observer_ && was_muted != mute) {
        observer_->OnMuteChanged(mute);
    }
    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::GetMute(bool* mute) const {
    if (!mute) return ErrorCode::kInvalidParam;
    *mute = mute_;
    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::SetPlaybackRate(float rate) {
    if (rate < 0.5f || rate > 2.0f) return ErrorCode::kInvalidParam;
    playback_rate_ = rate;
    param_.playback_rate = rate;
    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::GetPlaybackRate(float* rate) const {
    if (!rate) return ErrorCode::kInvalidParam;
    *rate = playback_rate_;
    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::SetBuffering(bool enable) {
    enable_buffering_ = enable;
    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::FlushBuffer() {
    std::lock_guard<std::mutex> lock(frame_queue_mutex_);
    while (!frame_queue_.empty()) {
        frame_queue_.pop();
    }
    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::GetBufferStatus(int* buffered_ms, int* queued_frames) const {
    if (!buffered_ms || !queued_frames) return ErrorCode::kInvalidParam;

    std::lock_guard<std::mutex> lock(frame_queue_mutex_);
    *queued_frames = static_cast<int>(frame_queue_.size());
    *buffered_ms = *queued_frames * 10;

    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::GetStats(AudioPlayStats* stats) const {
    if (!stats) return ErrorCode::kInvalidParam;

    std::lock_guard<std::mutex> lock(stats_mutex_);
    *stats = stats_;
    return ErrorCode::kOk;
}

ErrorCode FakeAudioPlayer::ResetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = AudioPlayStats();
    return ErrorCode::kOk;
}

void FakeAudioPlayer::ProcessFrame(const AudioFrame& frame) {
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    if (dist(gen) < drop_frame_rate_) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.overrun_count++;
        return;
    }

    if (play_latency_us_ > 0 && process_frames_) {
        std::this_thread::sleep_for(microseconds(play_latency_us_));
    }

    int64_t play_time_us = duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_frames_played++;
        stats_.total_bytes_played += frame.GetDataSize();
        stats_.current_sample_rate = frame.sample_rate;
        stats_.current_channels = frame.channels;
        stats_.current_volume = volume_;
    }

    if (observer_) {
        observer_->OnFramePlayed(frame, play_time_us);
    }
}

void FakeAudioPlayer::PlayThreadLoop() {
    while (playing_.load()) {
        AudioFrame frame;

        {
            std::unique_lock<std::mutex> lock(frame_queue_mutex_);
            frame_cv_.wait_for(lock, milliseconds(50), [this] {
                return !frame_queue_.empty() || !playing_.load();
            });

            if (!playing_.load()) break;

            if (!frame_queue_.empty()) {
                frame = std::move(frame_queue_.front());
                frame_queue_.pop();
            }
        }

        if (frame.sample_rate > 0 && frame.samples_per_channel > 0) {
            ProcessFrame(frame);
        }
    }
}

}  // namespace fake
}  // namespace minirtc
