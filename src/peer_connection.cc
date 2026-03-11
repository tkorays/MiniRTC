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
