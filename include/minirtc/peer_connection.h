#ifndef MINIRTC_PEER_CONNECTION_H_
#define MINIRTC_PEER_CONNECTION_H_

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "minirtc/stream_track.h"
#include "minirtc/ice.h"

namespace minirtc {

// PeerConnection配置
struct PeerConnectionConfig {
    // ICE配置
    std::vector<std::string> ice_servers = {
        "stun:stun.l.google.com:19302"
    };
    
    // 媒体配置
    bool enable_audio = true;
    bool enable_video = true;
    
    // 编码配置
    uint32_t audio_bitrate_bps = 48000;
    uint32_t video_bitrate_bps = 500000;
};

// PeerConnection状态
enum class PeerConnectionState {
    kNew,
    kConnecting,
    kConnected,
    kDisconnected,
    kFailed,
    kClosed
};

// IPeerConnectionHandler接口
class IPeerConnectionHandler {
public:
    virtual ~IPeerConnectionHandler() = default;
    virtual void OnConnectionStateChange(PeerConnectionState state) = 0;
    virtual void OnIceCandidate(const IceCandidate& candidate) = 0;
    virtual void OnTrackAdded(std::shared_ptr<ITrack> track) = 0;
};

// IPeerConnection接口
class IPeerConnection {
public:
    using Ptr = std::shared_ptr<IPeerConnection>;
    virtual ~IPeerConnection() = default;
    
    // 初始化
    virtual bool Initialize(const PeerConnectionConfig& config) = 0;
    
    // 设置回调
    virtual void SetHandler(std::shared_ptr<IPeerConnectionHandler> handler) = 0;
    
    // 添加本地Track
    virtual bool AddTrack(std::shared_ptr<ITrack> track) = 0;
    
    // 移除Track
    virtual bool RemoveTrack(uint32_t track_id) = 0;
    
    // 开始连接（收集ICE候选）
    virtual bool Start() = 0;
    
    // 停止连接
    virtual void Stop() = 0;
    
    // 设置远端ICE候选
    virtual bool AddIceCandidate(const IceCandidate& candidate) = 0;
    
    // 获取本地ICE候选
    virtual std::vector<IceCandidate> GetLocalCandidates() = 0;
    
    // 获取状态
    virtual PeerConnectionState GetState() const = 0;
};

// 创建PeerConnection
std::shared_ptr<IPeerConnection> CreatePeerConnection();

}  // namespace minirtc

#endif
