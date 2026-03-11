/**
 * @file peer_connection.cc
 * @brief MiniRTC PeerConnection implementation
 */

#include "minirtc/peer_connection.h"

#include <mutex>
#include <atomic>
#include <chrono>
#include <cassert>

namespace minirtc {

// ============================================================================
// PeerConnection Implementation
// ============================================================================

class PeerConnection : public IPeerConnection {
public:
    PeerConnection();
    ~PeerConnection() override;

    // IPeerConnection interface
    bool Initialize(const PeerConnectionConfig& config) override;
    void SetHandler(std::shared_ptr<IPeerConnectionHandler> handler) override;
    bool AddTrack(std::shared_ptr<ITrack> track) override;
    bool RemoveTrack(uint32_t track_id) override;
    bool Start() override;
    void Stop() override;
    bool AddIceCandidate(const IceCandidate& candidate) override;
    std::vector<IceCandidate> GetLocalCandidates() override;
    PeerConnectionState GetState() const override;
    std::unique_ptr<RTCStatsReport> GetStats() override;
    uint64_t GetSessionDurationMs() const override;

private:
    void SetState(PeerConnectionState state);
    void NotifyIceCandidate(const IceCandidate& candidate);
    void NotifyTrackAdded(std::shared_ptr<ITrack> track);
    void NotifyStateChange(PeerConnectionState state);

    // Configuration
    PeerConnectionConfig config_;
    
    // Handler
    std::shared_ptr<IPeerConnectionHandler> handler_;
    
    // State
    std::atomic<PeerConnectionState> state_;
    std::atomic<bool> is_initialized_;
    std::atomic<bool> is_running_;
    
    // ICE
    IIceAgent::Ptr ice_agent_;
    std::vector<IceCandidate> local_candidates_;
    std::vector<IceCandidate> remote_candidates_;
    
    // Tracks
    std::vector<std::shared_ptr<ITrack>> local_tracks_;
    
    // Session timing
    std::chrono::steady_clock::time_point session_start_time_;
    
    // Mutex
    mutable std::mutex mutex_;
};

// ============================================================================
// PeerConnection Implementation
// ============================================================================

PeerConnection::PeerConnection()
    : state_(PeerConnectionState::kNew)
    , is_initialized_(false)
    , is_running_(false) {
}

PeerConnection::~PeerConnection() {
    Stop();
}

bool PeerConnection::Initialize(const PeerConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (is_initialized_) {
        return false;
    }
    
    config_ = config;
    
    // Create ICE agent
    ice_agent_ = CreateIceAgent();
    if (!ice_agent_) {
        return false;
    }
    
    // Configure STUN
    StunConfig stun_config;
    stun_config.servers = config_.ice_servers;
    
    if (!ice_agent_->Initialize(stun_config)) {
        return false;
    }
    
    is_initialized_ = true;
    SetState(PeerConnectionState::kNew);
    
    return true;
}

void PeerConnection::SetHandler(std::shared_ptr<IPeerConnectionHandler> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    handler_ = handler;
}

bool PeerConnection::AddTrack(std::shared_ptr<ITrack> track) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!track) {
        return false;
    }
    
    // Check if track already exists
    for (const auto& t : local_tracks_) {
        if (t->GetId() == track->GetId()) {
            return false;
        }
    }
    
    local_tracks_.push_back(track);
    
    // Notify handler if available
    NotifyTrackAdded(track);
    
    return true;
}

bool PeerConnection::RemoveTrack(uint32_t track_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto it = local_tracks_.begin(); it != local_tracks_.end(); ++it) {
        if ((*it)->GetId() == track_id) {
            local_tracks_.erase(it);
            return true;
        }
    }
    
    return false;
}

bool PeerConnection::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_initialized_) {
        return false;
    }
    
    if (is_running_) {
        return false;
    }
    
    SetState(PeerConnectionState::kConnecting);
    
    // Record session start time
    session_start_time_ = std::chrono::steady_clock::now();
    
    // Start local tracks
    for (const auto& track : local_tracks_) {
        if (!track->Start()) {
            SetState(PeerConnectionState::kFailed);
            return false;
        }
    }
    
    // Gather ICE candidates
    if (ice_agent_) {
        local_candidates_ = ice_agent_->GatherCandidates();
        
        // Notify all local candidates
        for (const auto& candidate : local_candidates_) {
            NotifyIceCandidate(candidate);
        }
    }
    
    is_running_ = true;
    SetState(PeerConnectionState::kConnected);
    
    return true;
}

void PeerConnection::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_running_) {
        return;
    }
    
    // Stop local tracks
    for (const auto& track : local_tracks_) {
        track->Stop();
    }
    
    local_tracks_.clear();
    local_candidates_.clear();
    remote_candidates_.clear();
    
    is_running_ = false;
    SetState(PeerConnectionState::kClosed);
}

bool PeerConnection::AddIceCandidate(const IceCandidate& candidate) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_running_) {
        return false;
    }
    
    remote_candidates_.push_back(candidate);
    
    // TODO: Process ICE candidate pair and establish connection
    
    return true;
}

std::vector<IceCandidate> PeerConnection::GetLocalCandidates() {
    std::lock_guard<std::mutex> lock(mutex_);
    return local_candidates_;
}

PeerConnectionState PeerConnection::GetState() const {
    return state_.load();
}

std::unique_ptr<RTCStatsReport> PeerConnection::GetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto report = std::make_unique<RTCStatsReport>();
    report->timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Session duration
    uint64_t duration_ms = 0;
    if (session_start_time_.time_since_epoch().count() > 0) {
        auto now = std::chrono::steady_clock::now();
        duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - session_start_time_).count();
    }
    
    // Peer connection stats
    auto pc_stats = std::make_unique<PeerConnectionStats>();
    pc_stats->timestamp_ms = report->timestamp_ms;
    pc_stats->session_duration_ms = duration_ms;
    
    // Convert state to string
    switch (state_.load()) {
        case PeerConnectionState::kNew: pc_stats->state = "new"; break;
        case PeerConnectionState::kConnecting: pc_stats->state = "connecting"; break;
        case PeerConnectionState::kConnected: pc_stats->state = "connected"; break;
        case PeerConnectionState::kDisconnected: pc_stats->state = "disconnected"; break;
        case PeerConnectionState::kFailed: pc_stats->state = "failed"; break;
        case PeerConnectionState::kClosed: pc_stats->state = "closed"; break;
    }
    report->peer_connection_stats = std::move(pc_stats);
    
    // Transport stats
    auto transport_stats = std::make_unique<TransportStats>();
    transport_stats->timestamp_ms = report->timestamp_ms;
    transport_stats->transport_id = "default";
    // Aggregate transport stats from tracks
    for (const auto& track : local_tracks_) {
        auto track_stats = track->GetStats();
        transport_stats->packets_sent += track_stats.rtp_sent;
        transport_stats->packets_received += track_stats.rtp_received;
        transport_stats->bytes_sent += track_stats.bytes_sent;
        transport_stats->bytes_received += track_stats.bytes_received;
    }
    report->transport_stats = std::move(transport_stats);
    
    // Track stats
    for (const auto& track : local_tracks_) {
        auto track_stats = track->GetStats();
        
        if (track->GetKind() == MediaKind::kAudio) {
            AudioSenderStats audio_stats;
            audio_stats.timestamp_ms = report->timestamp_ms;
            audio_stats.track_id = track->GetId();
            audio_stats.ssrc = track->GetSsrc();
            audio_stats.packets_sent = track_stats.rtp_sent;
            audio_stats.bytes_sent = track_stats.bytes_sent;
            audio_stats.frames_encoded = track_stats.frames_encoded;
            audio_stats.encode_time_ms = track_stats.encode_time_ms;
            audio_stats.sample_rate = track_stats.sample_rate;
            audio_stats.channels = track_stats.channels;
            audio_stats.round_trip_time_ms = track_stats.round_trip_time_ms;
            audio_stats.jitter_ms = track_stats.jitter_ms;
            report->audio_sender_stats.push_back(audio_stats);
        } else if (track->GetKind() == MediaKind::kVideo) {
            VideoSenderStats video_stats;
            video_stats.timestamp_ms = report->timestamp_ms;
            video_stats.track_id = track->GetId();
            video_stats.ssrc = track->GetSsrc();
            video_stats.packets_sent = track_stats.rtp_sent;
            video_stats.bytes_sent = track_stats.bytes_sent;
            video_stats.frames_encoded = track_stats.frames_encoded;
            video_stats.key_frames_encoded = track_stats.key_frames_encoded;
            video_stats.encode_time_ms = track_stats.encode_time_ms;
            video_stats.frame_width = track_stats.frame_width;
            video_stats.frame_height = track_stats.frame_height;
            video_stats.frame_rate_sent = track_stats.frame_rate_sent;
            video_stats.round_trip_time_ms = track_stats.round_trip_time_ms;
            report->video_sender_stats.push_back(video_stats);
        }
    }
    
    return report;
}

uint64_t PeerConnection::GetSessionDurationMs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (session_start_time_.time_since_epoch().count() == 0) {
        return 0;
    }
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now - session_start_time_).count();
}

void PeerConnection::SetState(PeerConnectionState state) {
    PeerConnectionState old_state = state_.exchange(state);
    
    if (old_state != state) {
        NotifyStateChange(state);
    }
}

void PeerConnection::NotifyIceCandidate(const IceCandidate& candidate) {
    if (handler_) {
        handler_->OnIceCandidate(candidate);
    }
}

void PeerConnection::NotifyTrackAdded(std::shared_ptr<ITrack> track) {
    if (handler_) {
        handler_->OnTrackAdded(track);
    }
}

void PeerConnection::NotifyStateChange(PeerConnectionState state) {
    if (handler_) {
        handler_->OnConnectionStateChange(state);
    }
}

// ============================================================================
// Factory Function
// ============================================================================

std::shared_ptr<IPeerConnection> CreatePeerConnection() {
    return std::make_shared<PeerConnection>();
}

}  // namespace minirtc
