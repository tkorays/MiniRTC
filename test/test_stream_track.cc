/**
 * @file test_stream_track.cc
 * @brief Unit tests for Stream, Track, and StreamManager
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "minirtc/stream_track.h"
#include "minirtc/transport/rtp_packet.h"

using namespace minirtc;

// ============================================================================
// Track implementation for testing
// ============================================================================

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
    }

    void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) override {
        if (!packet) return;
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.rtp_received++;
    }

    TrackStats GetStats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

private:
    uint32_t id_;
    std::string name_;
    MediaKind kind_;
    uint32_t ssrc_;
    bool running_;
    mutable std::mutex mutex_;
    TrackStats stats_;
};

// ============================================================================
// Stream implementation for testing
// ============================================================================

class Stream : public IStream {
public:
    Stream(uint32_t id, const std::string& name)
        : id_(id), name_(name), running_(false) {}

    uint32_t GetId() const override { return id_; }
    std::string GetName() const override { return name_; }

    bool AddTrack(ITrack::Ptr track) override {
        if (!track) return false;
        std::lock_guard<std::mutex> lock(mutex_);
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

// ============================================================================
// StreamManager implementation for testing
// ============================================================================

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

    uint32_t GenerateTrackId() {
        return next_track_id_++;
    }

private:
    mutable std::mutex mutex_;
    uint32_t next_stream_id_;
    uint32_t next_track_id_;
    std::unordered_map<uint32_t, IStream::Ptr> streams_;
};

// ============================================================================
// Track Tests
// ============================================================================

class TrackTest : public ::testing::Test {
protected:
    void SetUp() override {
        track_ = std::make_shared<Track>(1, "video_track", MediaKind::kVideo, 0x12345678);
    }
    
    std::shared_ptr<Track> track_;
};

TEST_F(TrackTest, TrackCreation) {
    EXPECT_EQ(track_->GetId(), 1u);
    EXPECT_EQ(track_->GetName(), "video_track");
    EXPECT_EQ(track_->GetKind(), MediaKind::kVideo);
    EXPECT_EQ(track_->GetSsrc(), 0x12345678u);
    EXPECT_FALSE(track_->IsRunning());
}

TEST_F(TrackTest, TrackStartStop) {
    EXPECT_TRUE(track_->Start());
    EXPECT_TRUE(track_->IsRunning());
    
    // Start again should return false
    EXPECT_FALSE(track_->Start());
    
    track_->Stop();
    EXPECT_FALSE(track_->IsRunning());
}

TEST_F(TrackTest, TrackStats) {
    auto stats = track_->GetStats();
    EXPECT_EQ(stats.rtp_sent, 0u);
    EXPECT_EQ(stats.rtp_received, 0u);
    
    // Send a packet
    auto packet = std::make_shared<RtpPacket>(96, 1000, 1);
    track_->SendRtpPacket(packet);
    
    stats = track_->GetStats();
    EXPECT_EQ(stats.rtp_sent, 1u);
    
    // Receive a packet
    track_->OnRtpPacketReceived(packet);
    
    stats = track_->GetStats();
    EXPECT_EQ(stats.rtp_received, 1u);
}

// ============================================================================
// Stream Tests
// ============================================================================

class StreamTest : public ::testing::Test {
protected:
    void SetUp() override {
        stream_ = std::make_shared<Stream>(1, "test_stream");
        track_audio_ = std::make_shared<Track>(10, "audio_track", MediaKind::kAudio, 0x11111111);
        track_video_ = std::make_shared<Track>(20, "video_track", MediaKind::kVideo, 0x22222222);
    }
    
    std::shared_ptr<Stream> stream_;
    std::shared_ptr<Track> track_audio_;
    std::shared_ptr<Track> track_video_;
};

TEST_F(StreamTest, StreamCreation) {
    EXPECT_EQ(stream_->GetId(), 1u);
    EXPECT_EQ(stream_->GetName(), "test_stream");
    EXPECT_FALSE(stream_->IsRunning());
    EXPECT_EQ(stream_->GetTracks().size(), 0u);
}

TEST_F(StreamTest, AddTrack) {
    EXPECT_TRUE(stream_->AddTrack(track_audio_));
    EXPECT_EQ(stream_->GetTracks().size(), 1u);
    
    // Add duplicate track should fail
    EXPECT_FALSE(stream_->AddTrack(track_audio_));
}

TEST_F(StreamTest, RemoveTrack) {
    stream_->AddTrack(track_audio_);
    EXPECT_EQ(stream_->GetTracks().size(), 1u);
    
    EXPECT_TRUE(stream_->RemoveTrack(10));
    EXPECT_EQ(stream_->GetTracks().size(), 0u);
    
    // Remove non-existent track should fail
    EXPECT_FALSE(stream_->RemoveTrack(999));
}

TEST_F(StreamTest, GetTrackByKind) {
    stream_->AddTrack(track_audio_);
    stream_->AddTrack(track_video_);
    
    auto audio = stream_->GetTrack(MediaKind::kAudio);
    auto video = stream_->GetTrack(MediaKind::kVideo);
    
    EXPECT_NE(audio, nullptr);
    EXPECT_EQ(audio->GetId(), 10u);
    EXPECT_NE(video, nullptr);
    EXPECT_EQ(video->GetId(), 20u);
    
    // Non-existent kind
    auto none = stream_->GetTrack(MediaKind::kVideo);
    EXPECT_NE(none, nullptr);  // Already added
}

TEST_F(StreamTest, StreamStartStop) {
    stream_->AddTrack(track_audio_);
    stream_->AddTrack(track_video_);
    
    EXPECT_TRUE(stream_->Start());
    EXPECT_TRUE(stream_->IsRunning());
    EXPECT_TRUE(track_audio_->IsRunning());
    EXPECT_TRUE(track_video_->IsRunning());
    
    stream_->Stop();
    EXPECT_FALSE(stream_->IsRunning());
    EXPECT_FALSE(track_audio_->IsRunning());
    EXPECT_FALSE(track_video_->IsRunning());
}

TEST_F(StreamTest, StreamStats) {
    stream_->AddTrack(track_audio_);
    stream_->AddTrack(track_video_);
    
    auto stats = stream_->GetStats();
    EXPECT_EQ(stats.track_count, 2u);
}

// ============================================================================
// StreamManager Tests
// ============================================================================

class StreamManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_shared<StreamManager>();
    }
    
    std::shared_ptr<StreamManager> manager_;
};

TEST_F(StreamManagerTest, CreateStream) {
    auto stream1 = manager_->CreateStream("stream1");
    EXPECT_NE(stream1, nullptr);
    EXPECT_EQ(stream1->GetName(), "stream1");
    EXPECT_EQ(stream1->GetId(), 1u);
    
    auto stream2 = manager_->CreateStream("stream2");
    EXPECT_EQ(stream2->GetId(), 2u);
}

TEST_F(StreamManagerTest, GetStream) {
    auto stream1 = manager_->CreateStream("stream1");
    
    auto retrieved = manager_->GetStream(1);
    EXPECT_EQ(retrieved, stream1);
    
    // Non-existent
    auto none = manager_->GetStream(999);
    EXPECT_EQ(none, nullptr);
}

TEST_F(StreamManagerTest, DestroyStream) {
    auto stream1 = manager_->CreateStream("stream1");
    EXPECT_EQ(manager_->GetAllStreams().size(), 1u);
    
    EXPECT_TRUE(manager_->DestroyStream(1));
    EXPECT_EQ(manager_->GetAllStreams().size(), 0u);
    
    // Destroy non-existent
    EXPECT_FALSE(manager_->DestroyStream(999));
}

TEST_F(StreamManagerTest, GetAllStreams) {
    manager_->CreateStream("stream1");
    manager_->CreateStream("stream2");
    manager_->CreateStream("stream3");
    
    auto all = manager_->GetAllStreams();
    EXPECT_EQ(all.size(), 3u);
}

TEST_F(StreamManagerTest, StartAllStopAll) {
    auto stream1 = manager_->CreateStream("stream1");
    auto stream2 = manager_->CreateStream("stream2");
    
    auto track1 = std::make_shared<Track>(1, "track1", MediaKind::kAudio, 0x11111111);
    auto track2 = std::make_shared<Track>(2, "track2", MediaKind::kVideo, 0x22222222);
    
    stream1->AddTrack(track1);
    stream2->AddTrack(track2);
    
    manager_->StartAll();
    EXPECT_TRUE(stream1->IsRunning());
    EXPECT_TRUE(stream2->IsRunning());
    EXPECT_TRUE(track1->IsRunning());
    EXPECT_TRUE(track2->IsRunning());
    
    manager_->StopAll();
    EXPECT_FALSE(stream1->IsRunning());
    EXPECT_FALSE(stream2->IsRunning());
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
