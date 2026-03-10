/**
 * @file fake_audio_player.h
 * @brief Fake audio player implementation for testing
 */

#ifndef MINIRTC_FAKE_AUDIO_PLAYER_H
#define MINIRTC_FAKE_AUDIO_PLAYER_H

#include "capture_render.h"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace minirtc {
namespace fake {

// ============================================================================
// Fake Audio Player
// ============================================================================

class FakeAudioPlayer : public IAudioPlayer {
public:
    FakeAudioPlayer();
    ~FakeAudioPlayer() override;

    // Configuration
    void SetPlayLatency(int64_t latency_us);
    void SetDropFrameRate(float rate);
    void SetProcessFrames(bool process);

    // IAudioPlayer interface
    ErrorCode GetDevices(std::vector<AudioDeviceInfo>* devices) override;
    ErrorCode SetDevice(const std::string& device_id) override;
    std::string GetCurrentDevice() const override;

    ErrorCode SetParam(const AudioPlayParam& param) override;
    ErrorCode GetParam(AudioPlayParam* param) const override;

    ErrorCode Initialize() override;
    ErrorCode StartPlay(AudioPlayObserver* observer) override;
    ErrorCode StopPlay() override;
    ErrorCode Release() override;

    PlayState GetState() const override;
    bool IsPlaying() const override;
    bool IsBuffering() const override;

    ErrorCode PlayFrame(const AudioFrame& frame) override;
    ErrorCode PlayFrameList(const std::vector<AudioFrame>& frames) override;

    ErrorCode SetVolume(float volume) override;
    ErrorCode GetVolume(float* volume) const override;
    ErrorCode SetMute(bool mute) override;
    ErrorCode GetMute(bool* mute) const override;
    ErrorCode SetPlaybackRate(float rate) override;
    ErrorCode GetPlaybackRate(float* rate) const override;

    ErrorCode SetBuffering(bool enable) override;
    ErrorCode FlushBuffer() override;
    ErrorCode GetBufferStatus(int* buffered_ms, int* queued_frames) const override;

    ErrorCode GetStats(AudioPlayStats* stats) const override;
    ErrorCode ResetStats() override;

private:
    void ProcessFrame(const AudioFrame& frame);
    void PlayThreadLoop();

    AudioPlayObserver* observer_ = nullptr;
    AudioPlayParam param_;
    std::string device_id_;

    int64_t play_latency_us_ = 0;
    float drop_frame_rate_ = 0.0f;
    bool process_frames_ = true;

    float volume_ = 1.0f;
    bool mute_ = false;
    float playback_rate_ = 1.0f;

    bool buffering_ = false;
    bool enable_buffering_ = true;

    std::atomic<PlayState> state_{PlayState::kIdle};
    std::atomic<bool> playing_{false};
    std::thread play_thread_;

    mutable std::mutex stats_mutex_;
    AudioPlayStats stats_;

    mutable std::mutex frame_queue_mutex_;
    std::queue<AudioFrame> frame_queue_;
    std::condition_variable frame_cv_;
    std::atomic<bool> stop_processing_{false};
};

}  // namespace fake
}  // namespace minirtc

#endif  // MINIRTC_FAKE_AUDIO_PLAYER_H
