/**
 * @file media_synchronizer.h
 * @brief MiniRTC media synchronization module
 * @description Provides RTP timestamp synchronization, lip-sync, and playout scheduling
 */

#ifndef MINIRTC_MEDIA_SYNCHRONIZER_H_
#define MINIRTC_MEDIA_SYNCHRONIZER_H_

#include <cstdint>
#include <memory>

namespace minirtc {

// ============================================================================
// Media Synchronizer
// ============================================================================

/// Media synchronizer interface for RTP timestamp synchronization and lip-sync
class IMediaSynchronizer {
public:
    using Ptr = std::shared_ptr<IMediaSynchronizer>;

    virtual ~IMediaSynchronizer() = default;

    // ========================================================================
    // NTP Time Configuration
    // ========================================================================

    /// Set NTP time (wall clock time in milliseconds)
    /// @param ntp_ms NTP time in milliseconds since epoch
    virtual void SetNtpTime(uint64_t ntp_ms) = 0;

    /// Get current NTP time
    /// @return NTP time in milliseconds
    virtual uint64_t GetNtpTime() const = 0;

    // ========================================================================
    // RTP Timestamp Mapping
    // ========================================================================

    /// Set RTP timestamp for NTP time mapping
    /// @param rtp_timestamp RTP timestamp corresponding to current NTP time
    virtual void SetRtpTimestamp(uint32_t rtp_timestamp) = 0;

    /// Get RTP timestamp at given NTP time
    /// @param ntp_ms NTP time in milliseconds
    /// @return Estimated RTP timestamp
    virtual uint32_t GetRtpTimestampAtNtp(uint64_t ntp_ms) const = 0;

    // ========================================================================
    // Playout Time Calculation
    // ========================================================================

    /// Calculate playout time from RTP timestamp
    /// @param rtp_timestamp RTP timestamp
    /// @return Playout time in milliseconds (relative)
    virtual int64_t GetPlayTime(uint32_t rtp_timestamp) = 0;

    /// Calculate RTP timestamp from playout time
    /// @param play_time Playout time in milliseconds
    /// @return RTP timestamp
    virtual uint32_t GetRtpTimestampFromPlayTime(int64_t play_time) = 0;

    // ========================================================================
    // Audio/Video Sync
    // ========================================================================

    /// Set audio RTP timestamp at given NTP time
    /// @param audio_rtp_ts Audio RTP timestamp
    /// @param ntp_ms NTP time when this audio packet was captured
    virtual void SetAudioRtpTimestamp(uint32_t audio_rtp_ts, uint64_t ntp_ms) = 0;

    /// Set video RTP timestamp at given NTP time
    /// @param video_rtp_ts Video RTP timestamp
    /// @param ntp_ms NTP time when this video packet was captured
    virtual void SetVideoRtpTimestamp(uint32_t video_rtp_ts, uint64_t ntp_ms) = 0;

    /// Synchronize video to audio (lip-sync)
    /// @param audio_rtp_ts Audio RTP timestamp
    /// @param video_rtp_ts Video RTP timestamp
    /// @return true if synchronization is needed and applied
    virtual bool SyncVideoToAudio(uint32_t audio_rtp_ts, uint32_t video_rtp_ts) = 0;

    /// Get current audio-video sync offset
    /// @return Offset in milliseconds (positive = video ahead, negative = video behind)
    virtual int64_t GetAvSyncOffset() const = 0;

    /// Get the playout delay for media synchronization
    /// @return Delay in milliseconds
    virtual int GetPlayoutDelay() const = 0;

    /// Set the playout delay for media synchronization
    /// @param delay_ms Delay in milliseconds
    virtual void SetPlayoutDelay(int delay_ms) = 0;

    // ========================================================================
    // Clock Rate Configuration
    // ========================================================================

    /// Set audio clock rate (samples per second)
    /// @param clock_rate Audio clock rate (e.g., 48000, 44100)
    virtual void SetAudioClockRate(uint32_t clock_rate) = 0;

    /// Set video clock rate (ticks per second)
    /// @param clock_rate Video clock rate (typically 90000)
    virtual void SetVideoClockRate(uint32_t clock_rate) = 0;

    /// Get audio clock rate
    /// @return Audio clock rate
    virtual uint32_t GetAudioClockRate() const = 0;

    /// Get video clock rate
    /// @return Video clock rate
    virtual uint32_t GetVideoClockRate() const = 0;

    // ========================================================================
    // Statistics
    // ========================================================================

    /// Check if synchronization is ready
    /// @return true if NTP and RTP timestamp mapping is established
    virtual bool IsReady() const = 0;

    /// Reset synchronization state
    virtual void Reset() = 0;
};

// ============================================================================
// Factory Functions
// ============================================================================

/// Create media synchronizer
std::shared_ptr<IMediaSynchronizer> CreateMediaSynchronizer();

/// Create media synchronizer with clock rates
/// @param audio_clock_rate Audio clock rate (default 48000)
/// @param video_clock_rate Video clock rate (default 90000)
std::shared_ptr<IMediaSynchronizer> CreateMediaSynchronizer(
    uint32_t audio_clock_rate, 
    uint32_t video_clock_rate);

}  // namespace minirtc

#endif  // MINIRTC_MEDIA_SYNCHRONIZER_H_
