#ifndef MINIRTC_STREAM_TRACK_H_
#define MINIRTC_STREAM_TRACK_H_

#include <memory>
#include <string>
#include <vector>
#include "minirtc/transport/rtp_packet.h"

namespace minirtc {

// MediaKind
enum class MediaKind { kAudio = 1, kVideo = 2 };

// Track统计
struct TrackStats {
    uint64_t rtp_sent = 0;
    uint64_t rtp_received = 0;
};

// Stream统计
struct StreamStats {
    uint32_t track_count = 0;
};

// ITrack 接口
class ITrack : public std::enable_shared_from_this<ITrack> {
public:
    using Ptr = std::shared_ptr<ITrack>;
    virtual ~ITrack() = default;
    virtual MediaKind GetKind() const = 0;
    virtual uint32_t GetId() const = 0;
    virtual std::string GetName() const = 0;
    virtual uint32_t GetSsrc() const = 0;
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() const = 0;
    virtual void SendRtpPacket(std::shared_ptr<RtpPacket> packet) = 0;
    virtual void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) = 0;
    virtual TrackStats GetStats() const = 0;
};

// IStream 接口
class IStream : public std::enable_shared_from_this<IStream> {
public:
    using Ptr = std::shared_ptr<IStream>;
    virtual ~IStream() = default;
    virtual uint32_t GetId() const = 0;
    virtual std::string GetName() const = 0;
    virtual bool AddTrack(ITrack::Ptr track) = 0;
    virtual bool RemoveTrack(uint32_t track_id) = 0;
    virtual std::vector<ITrack::Ptr> GetTracks() const = 0;
    virtual ITrack::Ptr GetTrack(MediaKind kind) const = 0;
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() const = 0;
    virtual StreamStats GetStats() const = 0;
};

// IStreamManager 接口
class IStreamManager {
public:
    using Ptr = std::shared_ptr<IStreamManager>;
    virtual ~IStreamManager() = default;
    virtual IStream::Ptr CreateStream(const std::string& name) = 0;
    virtual bool DestroyStream(uint32_t stream_id) = 0;
    virtual IStream::Ptr GetStream(uint32_t stream_id) const = 0;
    virtual std::vector<IStream::Ptr> GetAllStreams() const = 0;
    virtual bool StartAll() = 0;
    virtual void StopAll() = 0;
};

}  // namespace minirtc

#endif  // MINIRTC_STREAM_TRACK_H_
