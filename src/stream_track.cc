#include "minirtc/stream_track.h"
#include <algorithm>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace minirtc {

// Track implementation
class Track : public ITrack {
public:
    Track(uint32_t id, const std::string& name, MediaKind kind, uint32_t ssrc)
        : id_(id), name_(name), kind_(kind), ssrc_(ssrc), running_(false) {}

    MediaKind GetKind() const override { return kind_; }
    uint32_t GetId() const override { return id_; }
    std::string GetName() const override { return name_; }
    uint32_t GetSsrc() const override { return ssrc_; }

    bool Start() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            start_time_ = std::chrono::steady_clock::now();
            running_ = true;
            return true;
        }
        return false;
    }

    void Stop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }

    bool IsRunning() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

    void SendRtpPacket(std::shared_ptr<RtpPacket> packet) override {
        if (!packet) return;
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.rtp_sent++;
        stats_.bytes_sent += packet->GetPayloadSize();
        // Update video stats if video track
        if (kind_ == MediaKind::kVideo) {
            // For simplicity, assume keyframe for now
            // In real implementation, check marker bit and frame type
        }
    }

    void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) override {
        if (!packet) return;
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.rtp_received++;
        stats_.bytes_received += packet->GetPayloadSize();
    }

    TrackStats GetStats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    // Extended stats update methods
    void RecordFrameEncoded(uint32_t encode_time_ms, bool is_keyframe = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.frames_encoded++;
        stats_.encode_time_ms = encode_time_ms;
        if (is_keyframe) {
            stats_.key_frames_encoded++;
        }
    }

    void RecordFrameDecoded(uint32_t decode_time_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.frames_decoded++;
        stats_.decode_time_ms = decode_time_ms;
    }

    void SetResolution(uint32_t width, uint32_t height) {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.frame_width = width;
        stats_.frame_height = height;
    }

    void SetAudioInfo(uint32_t sample_rate, uint32_t ch) {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.sample_rate = sample_rate;
        stats_.channels = ch;
    }

    void SetRtt(uint32_t rtt_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.round_trip_time_ms = rtt_ms;
    }

    void SetJitter(double jitter_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.jitter_ms = jitter_ms;
    }

    uint64_t GetSessionDurationMs() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (start_time_.time_since_epoch().count() == 0) {
            return 0;
        }
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();
    }

private:
    uint32_t id_;
    std::string name_;
    MediaKind kind_;
    uint32_t ssrc_;
    bool running_;
    mutable std::mutex mutex_;
    TrackStats stats_;
    std::chrono::steady_clock::time_point start_time_;
};

// Stream implementation
class Stream : public IStream {
public:
    Stream(uint32_t id, const std::string& name)
        : id_(id), name_(name), running_(false) {}

    uint32_t GetId() const override { return id_; }
    std::string GetName() const override { return name_; }

    bool AddTrack(ITrack::Ptr track) override {
        if (!track) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        // Check if track already exists
        for (const auto& t : tracks_) {
            if (t->GetId() == track->GetId()) {
                return false;
            }
        }
        tracks_.push_back(track);
        return true;
    }

    bool RemoveTrack(uint32_t track_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::remove_if(tracks_.begin(), tracks_.end(),
            [track_id](const ITrack::Ptr& t) { return t->GetId() == track_id; });
        if (it != tracks_.end()) {
            tracks_.erase(it, tracks_.end());
            return true;
        }
        return false;
    }

    std::vector<ITrack::Ptr> GetTracks() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        // Return copy to avoid race conditions
        return tracks_;
    }

    // GetTracksRef returns const reference (caller must hold mutex or accept race)
    const std::vector<ITrack::Ptr>& GetTracksRef() const {
        return tracks_;
    }

    ITrack::Ptr GetTrack(MediaKind kind) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& track : tracks_) {
            if (track->GetKind() == kind) {
                return track;
            }
        }
        return nullptr;
    }

    bool Start() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            running_ = true;
            for (const auto& track : tracks_) {
                track->Start();
            }
            return true;
        }
        return false;
    }

    void Stop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        for (const auto& track : tracks_) {
            track->Stop();
        }
    }

    bool IsRunning() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

    StreamStats GetStats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        StreamStats stats;
        stats.track_count = static_cast<uint32_t>(tracks_.size());
        return stats;
    }

private:
    uint32_t id_;
    std::string name_;
    bool running_;
    mutable std::mutex mutex_;
    std::vector<ITrack::Ptr> tracks_;
};

// StreamManager implementation
class StreamManager : public IStreamManager {
public:
    StreamManager() : next_stream_id_(1), next_track_id_(1) {}

    IStream::Ptr CreateStream(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t stream_id = next_stream_id_++;
        auto stream = std::make_shared<Stream>(stream_id, name);
        streams_[stream_id] = stream;
        return stream;
    }

    bool DestroyStream(uint32_t stream_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = streams_.find(stream_id);
        if (it != streams_.end()) {
            it->second->Stop();
            streams_.erase(it);
            return true;
        }
        return false;
    }

    IStream::Ptr GetStream(uint32_t stream_id) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = streams_.find(stream_id);
        if (it != streams_.end()) {
            return it->second;
        }
        return nullptr;
    }

    std::vector<IStream::Ptr> GetAllStreams() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<IStream::Ptr> result;
        for (const auto& pair : streams_) {
            result.push_back(pair.second);
        }
        return result;
    }

    bool StartAll() override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& pair : streams_) {
            pair.second->Start();
        }
        return true;
    }

    void StopAll() override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& pair : streams_) {
            pair.second->Stop();
        }
    }

    // Helper to generate unique track IDs
    uint32_t GenerateTrackId() {
        return next_track_id_++;
    }

private:
    mutable std::mutex mutex_;
    uint32_t next_stream_id_;
    uint32_t next_track_id_;
    std::unordered_map<uint32_t, IStream::Ptr> streams_;
};

// Factory functions
IStreamManager::Ptr CreateStreamManager() {
    return std::make_shared<StreamManager>();
}

}  // namespace minirtc
