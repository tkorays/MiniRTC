# MiniRTC 快速开始指南

## 简介

本指南将帮助你快速构建一个简单的音视频通话应用。

## 1. 简单音频通话

```cpp
#include <minirtc/peer_connection.h>
#include <minirtc/stream_track.h>
#include <memory>

class MyHandler : public IPeerConnectionHandler {
public:
    void OnConnectionStateChange(PeerConnectionState state) override {
        printf("Connection state: %d\n", static_cast<int>(state));
    }
    
    void OnIceCandidate(const IceCandidate& candidate) override {
        printf("ICE candidate: %s:%d\n", 
               candidate.host_addr.c_str(), candidate.port);
    }
    
    void OnTrackAdded(std::shared_ptr<ITrack> track) override {
        printf("Track added: %s\n", track->GetName().c_str());
    }
};

int main() {
    // 1. 创建PeerConnection
    auto pc = CreatePeerConnection();
    
    // 2. 设置回调
    auto handler = std::make_shared<MyHandler>();
    pc->SetHandler(handler);
    
    // 3. 配置
    PeerConnectionConfig config;
    config.enable_audio = true;
    config.enable_video = false;
    pc->Initialize(config);
    
    // 4. 启动
    pc->Start();
    
    // 等待...
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // 5. 停止
    pc->Stop();
    
    return 0;
}
```

## 2. 视频通话

```cpp
PeerConnectionConfig config;
config.enable_audio = true;
config.enable_video = true;
config.video_bitrate_bps = 1000000;  // 1Mbps
```

## 3. 手动创建Track

```cpp
// 创建音频Track
auto audio_track = std::make_shared<Track>(MediaKind::kAudio, "audio");
audio_track->SetSsrc(0x12345678);
pc->AddTrack(audio_track);

// 创建视频Track
auto video_track = std::make_shared<Track>(MediaKind::kVideo, "video");
video_track->SetSsrc(0x87654321);
pc->AddTrack(video_track);
```

## 4. 使用StreamManager

```cpp
auto manager = CreateStreamManager();

auto stream = manager->CreateStream("my-call");
stream->AddTrack(audio_track);
stream->AddTrack(video_track);

stream->Start();
// ... 通话进行中
stream->Stop();
```

## 下一步

- 查看 [API参考](API_REFERENCE.md) 了解更多接口
- 查看集成测试示例 `test/integration/`
