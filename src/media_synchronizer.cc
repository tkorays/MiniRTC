/**
 * @file media_synchronizer.cc
 * @brief MiniRTC media synchronization implementation
 */

#include "minirtc/media_synchronizer.h"

#include <cmath>
#include <algorithm>

namespace minirtc {

// ============================================================================
// MediaSynchronizer Implementation
// ============================================================================

class MediaSynchronizer : public IMediaSynchronizer {
public:
    MediaSynchronizer()
        : audio_clock_rate_(48000)
        , video_clock_rate_(90000)
        , playout_delay_ms_(0)
        , ntp_time_ms_(0)
        , base_rtp_timestamp_(0)
        , audio_base_ntp_ms_(0)
        , audio_base_rtp_ts_(0)
        , video_base_ntp_ms_(0)
        , video_base_rtp_ts_(0)
        , av_sync_offset_ms_(0)
        , is_ready_(false) {
    }

    MediaSynchronizer(uint32_t audio_clock_rate, uint32_t video_clock_rate)
        : audio_clock_rate_(audio_clock_rate)
        , video_clock_rate_(video_clock_rate)
        , playout_delay_ms_(0)
        , ntp_time_ms_(0)
        , base_rtp_timestamp_(0)
        , audio_base_ntp_ms_(0)
        , audio_base_rtp_ts_(0)
        , video_base_ntp_ms_(0)
        , video_base_rtp_ts_(0)
        , av_sync_offset_ms_(0)
        , is_ready_(false) {
    }

    ~MediaSynchronizer() override = default;

    // ========================================================================
    // NTP Time Configuration
    // ========================================================================

    void SetNtpTime(uint64_t ntp_ms) override {
        ntp_time_ms_ = ntp_ms;
    }

    uint64_t GetNtpTime() const override {
        return ntp_time_ms_;
    }

    // ========================================================================
    // RTP Timestamp Mapping
    // ========================================================================

    void SetRtpTimestamp(uint32_t rtp_timestamp) override {
        base_rtp_timestamp_ = rtp_timestamp;
        is_ready_ = (ntp_time_ms_ != 0);
    }

    uint32_t GetRtpTimestampAtNtp(uint64_t ntp_ms) const override {
        if (ntp_time_ms_ == 0 || base_rtp_timestamp_ == 0) {
            return 0;
        }

        int64_t delta_ms = static_cast<int64_t>(ntp_ms) - static_cast<int64_t>(ntp_time_ms_);
        
        // Calculate RTP timestamp increment based on audio clock rate
        // RTP timestamp increments by samples per second
        uint32_t ts_increment = static_cast<uint32_t>(
            (static_cast<int64_t>(audio_clock_rate_) * delta_ms) / 1000
        );

        return base_rtp_timestamp_ + ts_increment;
    }

    // ========================================================================
    // Playout Time Calculation
    // ========================================================================

    int64_t GetPlayTime(uint32_t rtp_timestamp) override {
        if (base_rtp_timestamp_ == 0 || audio_clock_rate_ == 0) {
            return 0;
        }

        // Calculate time difference in RTP timestamp units
        int64_t delta_ts = static_cast<int64_t>(rtp_timestamp) - 
                          static_cast<int64_t>(base_rtp_timestamp_);

        // Convert to milliseconds
        // timestamp delta / (clock_rate / 1000) = milliseconds
        int64_t play_time_ms = (delta_ts * 1000) / static_cast<int64_t>(audio_clock_rate_);

        // Add playout delay
        play_time_ms += playout_delay_ms_;

        return play_time_ms;
    }

    uint32_t GetRtpTimestampFromPlayTime(int64_t play_time) override {
        if (audio_clock_rate_ == 0) {
            return 0;
        }

        // Subtract playout delay
        int64_t adjusted_time = play_time - playout_delay_ms_;

        // Convert milliseconds to RTP timestamp
        // play_time_ms * (clock_rate / 1000) = RTP timestamp delta
        int64_t delta_ts = (adjusted_time * static_cast<int64_t>(audio_clock_rate_)) / 1000;

        return static_cast<uint32_t>(static_cast<int64_t>(base_rtp_timestamp_) + delta_ts);
    }

    // ========================================================================
    // Audio/Video Sync
    // ========================================================================

    void SetAudioRtpTimestamp(uint32_t audio_rtp_ts, uint64_t ntp_ms) override {
        audio_base_rtp_ts_ = audio_rtp_ts;
        audio_base_ntp_ms_ = ntp_ms;
    }

    void SetVideoRtpTimestamp(uint32_t video_rtp_ts, uint64_t ntp_ms) override {
        video_base_rtp_ts_ = video_rtp_ts;
        video_base_ntp_ms_ = ntp_ms;
    }

    bool SyncVideoToAudio(uint32_t audio_rtp_ts, uint32_t video_rtp_ts) override {
        // Calculate audio play time
        int64_t audio_play_time = GetPlayTime(audio_rtp_ts);
        
        // Calculate video play time (using video clock rate)
        if (video_clock_rate_ == 0 || base_rtp_timestamp_ == 0) {
            return false;
        }

        int64_t delta_ts = static_cast<int64_t>(video_rtp_ts) - 
                          static_cast<int64_t>(base_rtp_timestamp_);
        int64_t video_play_time_ms = (delta_ts * 1000) / static_cast<int64_t>(video_clock_rate_);
        video_play_time_ms += playout_delay_ms_;

        // Calculate offset
        av_sync_offset_ms_ = video_play_time_ms - audio_play_time;

        return true;
    }

    int64_t GetAvSyncOffset() const override {
        return av_sync_offset_ms_;
    }

    int GetPlayoutDelay() const override {
        return playout_delay_ms_;
    }

    void SetPlayoutDelay(int delay_ms) override {
        playout_delay_ms_ = delay_ms;
    }

    // ========================================================================
    // Clock Rate Configuration
    // ========================================================================

    void SetAudioClockRate(uint32_t clock_rate) override {
        audio_clock_rate_ = clock_rate;
    }

    void SetVideoClockRate(uint32_t clock_rate) override {
        video_clock_rate_ = clock_rate;
    }

    uint32_t GetAudioClockRate() const override {
        return audio_clock_rate_;
    }

    uint32_t GetVideoClockRate() const override {
        return video_clock_rate_;
    }

    // ========================================================================
    // Statistics
    // ========================================================================

    bool IsReady() const override {
        return is_ready_ && (base_rtp_timestamp_ != 0);
    }

    void Reset() override {
        ntp_time_ms_ = 0;
        base_rtp_timestamp_ = 0;
        audio_base_ntp_ms_ = 0;
        audio_base_rtp_ts_ = 0;
        video_base_ntp_ms_ = 0;
        video_base_rtp_ts_ = 0;
        av_sync_offset_ms_ = 0;
        is_ready_ = false;
    }

private:
    // Clock rates
    uint32_t audio_clock_rate_;
    uint32_t video_clock_rate_;

    // Playout delay
    int playout_delay_ms_;

    // NTP/RTP mapping
    uint64_t ntp_time_ms_;
    uint32_t base_rtp_timestamp_;

    // Audio/Video base timestamps for sync
    uint64_t audio_base_ntp_ms_;
    uint32_t audio_base_rtp_ts_;
    uint64_t video_base_ntp_ms_;
    uint32_t video_base_rtp_ts_;

    // AV sync offset (positive = video ahead)
    int64_t av_sync_offset_ms_;

    // Ready flag
    bool is_ready_;
};

// ============================================================================
// Factory Functions
// ============================================================================

std::shared_ptr<IMediaSynchronizer> CreateMediaSynchronizer() {
    return std::make_shared<MediaSynchronizer>();
}

std::shared_ptr<IMediaSynchronizer> CreateMediaSynchronizer(
    uint32_t audio_clock_rate, 
    uint32_t video_clock_rate) {
    return std::make_shared<MediaSynchronizer>(audio_clock_rate, video_clock_rate);
}

}  // namespace minirtc
