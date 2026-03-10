/**
 * @file fake_audio_capture.h
 * @brief Fake audio capture implementation for testing
 */

#ifndef MINIRTC_FAKE_AUDIO_CAPTURE_H
#define MINIRTC_FAKE_AUDIO_CAPTURE_H

#include "capture_render.h"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <random>

namespace minirtc {
namespace fake {

// ============================================================================
// Audio Frame Generator
// ============================================================================

class AudioFrameGenerator {
public:
    virtual ~AudioFrameGenerator() = default;
    virtual void GenerateFrame(AudioFrame* frame) = 0;
};

class SineWaveGenerator : public AudioFrameGenerator {
public:
    SineWaveGenerator(int sample_rate, int frequency_hz, int channels);
    void GenerateFrame(AudioFrame* frame) override;

private:
    int sample_rate_;
    int frequency_hz_;
    int channels_;
    int64_t sample_index_ = 0;
};

class SilenceGenerator : public AudioFrameGenerator {
public:
    SilenceGenerator(int sample_rate, int channels);
    void GenerateFrame(AudioFrame* frame) override;

private:
    int sample_rate_;
    int channels_;
};

class WhiteNoiseGenerator : public AudioFrameGenerator {
public:
    WhiteNoiseGenerator(int sample_rate, int channels);
    void GenerateFrame(AudioFrame* frame) override;

private:
    int sample_rate_;
    int channels_;
};

// ============================================================================
// Fake Audio Capture
// ============================================================================

class FakeAudioCapture : public IAudioCapture {
public:
    FakeAudioCapture();
    ~FakeAudioCapture() override;

    // Configuration
    void SetFrameGenerator(std::unique_ptr<AudioFrameGenerator> generator);
    void SetGenerateSineWave(int frequency_hz = 440);
    void SetSilence();
    void SetWhiteNoise();
    void SetGenerateVolume(float volume_db);

    // IAudioCapture interface
    ErrorCode GetDevices(std::vector<AudioDeviceInfo>* devices) override;
    ErrorCode SetDevice(const std::string& device_id) override;
    std::string GetCurrentDevice() const override;

    ErrorCode SetParam(const AudioCaptureParam& param) override;
    ErrorCode GetParam(AudioCaptureParam* param) const override;

    ErrorCode Initialize() override;
    ErrorCode StartCapture(AudioCaptureObserver* observer) override;
    ErrorCode StopCapture() override;
    ErrorCode Release() override;

    CaptureState GetState() const override;
    bool IsCapturing() const override;

    ErrorCode GetStats(AudioCaptureStats* stats) const override;
    ErrorCode ResetStats() override;

    ErrorCode SetVolume(float volume) override;
    ErrorCode GetVolume(float* volume) const override;
    ErrorCode SetMute(bool mute) override;
    ErrorCode GetMute(bool* mute) const override;

    ErrorCode SetEchoReference(std::shared_ptr<AudioFrame> frame) override;

private:
    void GenerateAndPushFrame();
    void CaptureThreadLoop();

    AudioCaptureObserver* observer_ = nullptr;
    std::unique_ptr<AudioFrameGenerator> frame_generator_;
    AudioCaptureParam param_;
    std::string device_id_;

    float volume_ = 1.0f;
    bool mute_ = false;

    std::atomic<CaptureState> state_{CaptureState::kIdle};
    std::atomic<bool> capturing_{false};
    std::thread capture_thread_;

    mutable std::mutex stats_mutex_;
    AudioCaptureStats stats_;

    std::atomic<uint16_t> seq_num_{0};
    std::atomic<uint32_t> timestamp_rtp_{0};
    std::atomic<int64_t> start_time_us_{0};
};

}  // namespace fake
}  // namespace minirtc

#endif  // MINIRTC_FAKE_AUDIO_CAPTURE_H
