#ifndef MINIRTC_STATS_H_
#define MINIRTC_STATS_H_

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

namespace minirtc {

// ============================================================================
// Basic Types
// ============================================================================

// Media type enum
enum class MediaType {
    kAudio,
    kVideo
};

// Stats type enum
enum class RTCStatsType {
    kPeerConnection,
    kTrack,
    kSender,
    kReceiver,
    kTransport,
    kCodec
};

// Base stats timestamp
struct RTCStatsMember {
    int64_t timestamp_ms = 0;
};

// ============================================================================
// Audio Stats
// ============================================================================

// Audio sender stats
struct AudioSenderStats : public RTCStatsMember {
    RTCStatsType type = RTCStatsType::kSender;
    uint32_t track_id = 0;
    std::string track_kind = "audio";
    
    // Encoding stats
    uint64_t frames_encoded = 0;
    uint64_t bytes_encoded = 0;
    uint32_t encode_time_ms = 0;
    uint32_t encode_errors = 0;
    
    // Send stats
    uint64_t packets_sent = 0;
    uint64_t bytes_sent = 0;
    uint32_t ssrc = 0;
    
    // Target bitrate
    uint32_t target_bitrate_bps = 0;
    
    // Audio quality
    uint32_t sample_rate = 0;
    uint32_t channels = 0;
    double jitter_ms = 0.0;
    uint32_t round_trip_time_ms = 0;
};

// Audio receiver stats
struct AudioReceiverStats : public RTCStatsMember {
    RTCStatsType type = RTCStatsType::kReceiver;
    uint32_t track_id = 0;
    std::string track_kind = "audio";
    
    // Receive stats
    uint64_t packets_received = 0;
    uint64_t bytes_received = 0;
    uint64_t packets_lost = 0;
    uint32_t ssrc = 0;
    
    // Decode stats
    uint64_t frames_decoded = 0;
    uint64_t frames_rendered = 0;
    uint32_t decode_time_ms = 0;
    uint32_t decode_errors = 0;
    
    // Jitter and delay
    double jitter_ms = 0.0;
    uint32_t round_trip_time_ms = 0;
    
    // Audio quality
    uint32_t sample_rate = 0;
    uint32_t channels = 0;
    float audio_level = 0.0;
    float total_audio_energy = 0.0;
    uint64_t total_samples_duration = 0;
};

// ============================================================================
// Video Stats
// ============================================================================

// Video sender stats
struct VideoSenderStats : public RTCStatsMember {
    RTCStatsType type = RTCStatsType::kSender;
    uint32_t track_id = 0;
    std::string track_kind = "video";
    
    // Encoding stats
    uint64_t frames_encoded = 0;
    uint64_t bytes_encoded = 0;
    uint32_t encode_time_ms = 0;
    uint32_t encode_errors = 0;
    
    // Frame type stats
    uint64_t key_frames_encoded = 0;
    uint64_t delta_frames_encoded = 0;
    
    // Send stats
    uint64_t packets_sent = 0;
    uint64_t bytes_sent = 0;
    uint32_t ssrc = 0;
    
    // Bitrate control
    uint32_t target_bitrate_bps = 0;
    uint32_t actual_bitrate_bps = 0;
    
    // Frame rate
    double frame_rate_input = 0.0;
    double frame_rate_sent = 0.0;
    
    // Resolution
    uint32_t frame_width = 0;
    uint32_t frame_height = 0;
    uint32_t framesize_scale = 1;
    
    // Jitter and RTT
    double jitter_ms = 0.0;
    uint32_t round_trip_time_ms = 0;
};

// Video receiver stats
struct VideoReceiverStats : public RTCStatsMember {
    RTCStatsType type = RTCStatsType::kReceiver;
    uint32_t track_id = 0;
    std::string track_kind = "video";
    
    // Receive stats
    uint64_t packets_received = 0;
    uint64_t bytes_received = 0;
    uint64_t packets_lost = 0;
    uint32_t ssrc = 0;
    
    // Decode stats
    uint64_t frames_decoded = 0;
    uint64_t frames_rendered = 0;
    uint32_t decode_time_ms = 0;
    uint32_t decode_errors = 0;
    uint64_t freeze_count = 0;
    
    // Jitter and delay
    double jitter_ms = 0.0;
    uint32_t round_trip_time_ms = 0;
    
    // Resolution
    uint32_t frame_width = 0;
    uint32_t frame_height = 0;
    
    // Frame rate
    double frame_rate_received = 0.0;
    double frame_rate_decoded = 0.0;
    double frame_rate_rendered = 0.0;
};

// ============================================================================
// Transport Stats
// ============================================================================

// Transport stats
struct TransportStats : public RTCStatsMember {
    RTCStatsType type = RTCStatsType::kTransport;
    std::string transport_id;
    
    // SCTP/UDP stats
    uint64_t packets_sent = 0;
    uint64_t packets_received = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    
    // RTT
    uint32_t round_trip_time_ms = 0;
};

// ============================================================================
// Peer Connection Stats
// ============================================================================

// Peer connection stats
struct PeerConnectionStats : public RTCStatsMember {
    RTCStatsType type = RTCStatsType::kPeerConnection;
    
    // Connection state
    std::string state;
    uint64_t data_channels_opened = 0;
    uint64_t data_channels_closed = 0;
    
    // Session duration
    uint64_t session_duration_ms = 0;
};

// ============================================================================
// Unified Stats Report
// ============================================================================

// Stats report
struct RTCStatsReport {
    int64_t timestamp_ms = 0;
    
    // Session stats
    std::unique_ptr<PeerConnectionStats> peer_connection_stats;
    
    // Transport stats
    std::unique_ptr<TransportStats> transport_stats;
    
    // Sender stats
    std::vector<AudioSenderStats> audio_sender_stats;
    std::vector<VideoSenderStats> video_sender_stats;
    
    // Receiver stats
    std::vector<AudioReceiverStats> audio_receiver_stats;
    std::vector<VideoReceiverStats> video_receiver_stats;
};

}  // namespace minirtc

#endif  // MINIRTC_STATS_H_
