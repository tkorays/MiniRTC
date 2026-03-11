/**
 * @file minirtc_demo.cc
 * @brief MiniRTC command-line demo for 1-to-1 audio/video call
 */

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <getopt.h>

#include "minirtc/peer_connection.h"
#include "minirtc/capture_render.h"
#include "minirtc/stream_track.h"
#include "minirtc/transport/rtp_packet.h"

using namespace minirtc;

// ============================================================================
// Global state
// ============================================================================

std::atomic<bool> g_running{false};

// ============================================================================
// Signal handler
// ============================================================================

void signal_handler(int signal) {
    std::cout << "\n收到退出信号，正在停止..." << std::endl;
    g_running = false;
}

// ============================================================================
// Track implementations
// ============================================================================

class AudioTrack : public ITrack, public AudioCaptureObserver, public AudioPlayObserver {
public:
    AudioTrack(uint32_t id, const std::string& name, uint32_t ssrc,
               std::unique_ptr<IAudioCapture> capture,
               std::unique_ptr<IAudioPlayer> player)
        : id_(id), name_(name), ssrc_(ssrc), 
          capture_(std::move(capture)), player_(std::move(player)),
          running_(false) {
        
        // Initialize capture
        if (capture_) {
            capture_->Initialize();
            AudioCaptureParam param;
            param.sample_rate = 48000;
            param.channels = 2;
            capture_->SetParam(param);
        }
        
        // Initialize player
        if (player_) {
            player_->Initialize();
            AudioPlayParam param;
            param.sample_rate = 48000;
            param.channels = 2;
            player_->SetParam(param);
        }
    }
    
    ~AudioTrack() override {
        Stop();
        if (capture_) capture_->Release();
        if (player_) player_->Release();
    }
    
    MediaKind GetKind() const override { return MediaKind::kAudio; }
    uint32_t GetId() const override { return id_; }
    std::string GetName() const override { return name_; }
    uint32_t GetSsrc() const override { return ssrc_; }
    
    bool Start() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            // Start capture
            if (capture_) {
                capture_->StartCapture(this);
            }
            // Start player
            if (player_) {
                player_->StartPlay(this);
            }
            running_ = true;
            return true;
        }
        return false;
    }
    
    void Stop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            if (capture_) capture_->StopCapture();
            if (player_) player_->StopPlay();
            running_ = false;
        }
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
    
    // AudioCaptureObserver
    void OnFrameCaptured(const AudioFrame& frame) override {
        // In loopback mode, play the captured frame
        if (player_ && running_) {
            player_->PlayFrame(frame);
        }
    }
    
    void OnVolumeChanged(float volume_db) override {}
    void OnMuteDetected(bool is_muted) override {}
    void OnCaptureError(ErrorCode error_code, const std::string& error_msg) override {
        std::cerr << "Audio capture error: " << error_msg << std::endl;
    }
    void OnDeviceChanged() override {}
    
    // AudioPlayObserver  
    void OnFramePlayed(const AudioFrame& frame, int64_t play_time_us) override {}
    void OnPlayError(ErrorCode error_code, const std::string& error_msg) override {
        std::cerr << "Audio play error: " << error_msg << std::endl;
    }
    void OnPlayStats(const AudioPlayStats& stats) override {}
    void OnBufferingChanged(bool is_buffering) override {}
    void OnMuteChanged(bool is_muted) override {}
    
private:
    uint32_t id_;
    std::string name_;
    uint32_t ssrc_;
    std::unique_ptr<IAudioCapture> capture_;
    std::unique_ptr<IAudioPlayer> player_;
    bool running_;
    mutable std::mutex mutex_;
    TrackStats stats_;
};

class VideoTrack : public ITrack, public VideoCaptureObserver, public VideoRenderObserver {
public:
    VideoTrack(uint32_t id, const std::string& name, uint32_t ssrc,
               std::unique_ptr<IVideoCapture> capture,
               std::unique_ptr<IVideoRenderer> renderer)
        : id_(id), name_(name), ssrc_(ssrc),
          capture_(std::move(capture)), renderer_(std::move(renderer)),
          running_(false) {
        
        // Initialize capture
        if (capture_) {
            capture_->Initialize();
            VideoCaptureParam param;
            param.width = 640;
            param.height = 480;
            param.target_fps = 30;
            capture_->SetParam(param);
        }
        
        // Initialize renderer
        if (renderer_) {
            VideoRenderParam param;
            param.output_width = 640;
            param.output_height = 480;
            renderer_->Initialize(param);
        }
    }
    
    ~VideoTrack() override {
        Stop();
        if (capture_) capture_->Release();
        if (renderer_) renderer_->Release();
    }
    
    MediaKind GetKind() const override { return MediaKind::kVideo; }
    uint32_t GetId() const override { return id_; }
    std::string GetName() const override { return name_; }
    uint32_t GetSsrc() const override { return ssrc_; }
    
    bool Start() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            // Start capture
            if (capture_) {
                std::weak_ptr<VideoCaptureObserver> observer = 
                    std::dynamic_pointer_cast<VideoCaptureObserver>(
                        shared_from_this());
                capture_->StartCapture(observer);
            }
            // Start renderer
            if (renderer_) {
                renderer_->StartRender(this);
            }
            running_ = true;
            return true;
        }
        return false;
    }
    
    void Stop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            if (capture_) capture_->StopCapture();
            if (renderer_) renderer_->StopRender();
            running_ = false;
        }
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
        
        // Render received frame
        if (renderer_ && running_) {
            // Would decode and render here
        }
    }
    
    TrackStats GetStats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
    
    // VideoCaptureObserver
    void OnFrameCaptured(const VideoFrame& frame) override {
        // In loopback mode, render the captured frame
        if (renderer_ && running_) {
            renderer_->RenderFrame(frame);
        }
    }
    
    void OnCaptureError(ErrorCode error_code, const std::string& error_msg) override {
        std::cerr << "Video capture error: " << error_msg << std::endl;
    }
    
    void OnDeviceChanged() override {}
    
    // VideoRenderObserver
    void OnFrameRendered(const VideoFrame& frame, int64_t render_time_us) override {}
    void OnRenderError(ErrorCode error_code, const std::string& error_msg) override {
        std::cerr << "Video render error: " << error_msg << std::endl;
    }
    void OnRenderStats(const VideoRenderStats& stats) override {}
    
private:
    uint32_t id_;
    std::string name_;
    uint32_t ssrc_;
    std::unique_ptr<IVideoCapture> capture_;
    std::unique_ptr<IVideoRenderer> renderer_;
    bool running_;
    mutable std::mutex mutex_;
    TrackStats stats_;
};

// ============================================================================
// PeerConnection handler for demo
// ============================================================================

class DemoHandler : public IPeerConnectionHandler {
public:
    void OnConnectionStateChange(PeerConnectionState state) override {
        std::cout << "[Demo] Connection state: ";
        switch (state) {
            case PeerConnectionState::kNew: std::cout << "New"; break;
            case PeerConnectionState::kConnecting: std::cout << "Connecting"; break;
            case PeerConnectionState::kConnected: std::cout << "Connected"; break;
            case PeerConnectionState::kDisconnected: std::cout << "Disconnected"; break;
            case PeerConnectionState::kFailed: std::cout << "Failed"; break;
            case PeerConnectionState::kClosed: std::cout << "Closed"; break;
        }
        std::cout << std::endl;
    }
    
    void OnIceCandidate(const IceCandidate& candidate) override {
        std::cout << "[Demo] ICE candidate: " << candidate.host_addr 
                  << ":" << candidate.port << std::endl;
    }
    
    void OnTrackAdded(std::shared_ptr<ITrack> track) override {
        std::cout << "[Demo] Track added: " << track->GetName() 
                  << " (kind: " << (track->GetKind() == MediaKind::kAudio ? "audio" : "video") 
                  << ")" << std::endl;
    }
};

// ============================================================================
// Print usage
// ============================================================================

void print_usage(const char* program_name) {
    std::cout << "MiniRTC Demo - 1-to-1 Audio/Video Call\n\n"
              << "Usage: " << program_name << " [options]\n\n"
              << "Options:\n"
              << "  -m, --mode MODE       运行模式: loopback, caller, callee (默认: loopback)\n"
              << "  -v, --video           启用视频 (默认: 开启)\n"
              << "  -a, --audio           启用音频 (默认: 开启)\n"
              << "  -w, --width WIDTH     视频宽度 (默认: 640)\n"
              << "  -h, --height HEIGHT   视频高度 (默认: 480)\n"
              << "  -f, --fps FPS         视频帧率 (默认: 30)\n"
              << "      --no-video        禁用视频\n"
              << "      --no-audio        禁用音频\n"
              << "  -?, --help            显示帮助\n\n"
              << "Examples:\n"
              << "  " << program_name << "                    # Loopback模式 (音视频)\n"
              << "  " << program_name << " -m loopback        # 回环测试\n"
              << "  " << program_name << " --no-video         # 仅音频\n"
              << "  " << program_name << " -a -v -m caller    # 作为呼叫方\n";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    // Parse command line arguments
    bool enable_video = true;
    bool enable_audio = true;
    std::string mode = "loopback";
    int width = 640;
    int height = 480;
    int fps = 30;
    
    static struct option long_options[] = {
        {"mode", required_argument, 0, 'm'},
        {"video", no_argument, 0, 'v'},
        {"audio", no_argument, 0, 'a'},
        {"width", required_argument, 0, 'w'},
        {"height", required_argument, 0, 'h'},
        {"fps", required_argument, 0, 'f'},
        {"no-video", no_argument, 0, 1},
        {"no-audio", no_argument, 0, 2},
        {"help", no_argument, 0, '?'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "m:vah:w:f:", 
                               long_options, &option_index)) != -1) {
        switch (opt) {
            case 'm':
                mode = optarg;
                break;
            case 'v':
                enable_video = true;
                break;
            case 'a':
                enable_audio = true;
                break;
            case 1:  // --no-video
                enable_video = false;
                break;
            case 2:  // --no-audio
                enable_audio = false;
                break;
            case 'w':
                try {
                    width = std::stoi(optarg);
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid width value: " << optarg << std::endl;
                    return 1;
                }
                break;
            case 'h':
                try {
                    height = std::stoi(optarg);
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid height value: " << optarg << std::endl;
                    return 1;
                }
                break;
            case 'f':
                try {
                    fps = std::stoi(optarg);
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid fps value: " << optarg << std::endl;
                    return 1;
                }
                break;
            case '?':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Validate mode
    if (mode != "loopback" && mode != "caller" && mode != "callee") {
        std::cerr << "Error: Invalid mode '" << mode << "'\n";
        print_usage(argv[0]);
        return 1;
    }
    
    // Check if at least one media is enabled
    if (!enable_video && !enable_audio) {
        std::cerr << "Error: Both video and audio are disabled\n";
        return 1;
    }
    
    std::cout << "===========================================\n";
    std::cout << "  MiniRTC Demo\n";
    std::cout << "===========================================\n";
    std::cout << "Mode:     " << mode << "\n";
    std::cout << "Video:    " << (enable_video ? "enabled" : "disabled") 
              << " (" << width << "x" << height << "@" << fps << "fps)\n";
    std::cout << "Audio:    " << (enable_audio ? "enabled" : "disabled") << "\n";
    std::cout << "===========================================\n\n";
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create handler first and keep it alive (shared_ptr ensures lifecycle)
    auto handler = std::make_shared<DemoHandler>();
    
    // Create PeerConnection
    std::cout << "[1/5] 创建PeerConnection..." << std::endl;
    auto pc = CreatePeerConnection();
    if (!pc) {
        std::cerr << "Failed to create PeerConnection\n";
        return 1;
    }
    
    // Configure PeerConnection
    std::cout << "[2/5] 配置PeerConnection..." << std::endl;
    PeerConnectionConfig config;
    config.enable_video = enable_video;
    config.enable_audio = enable_audio;
    config.audio_bitrate_bps = 48000;
    config.video_bitrate_bps = 500000;
    
    if (!pc->Initialize(config)) {
        std::cerr << "Failed to initialize PeerConnection\n";
        return 1;
    }
    
    // Set handler (handler is kept alive by shared_ptr in this scope)
    pc->SetHandler(handler);
    
    // Create and add tracks
    std::cout << "[3/5] 创建媒体Track..." << std::endl;
    
    if (enable_audio) {
        auto capture = IAudioCapture::Create();
        auto player = IAudioPlayer::Create();
        auto audio_track = std::make_shared<AudioTrack>(
            1, "audio_track", 1001, 
            std::move(capture), std::move(player)
        );
        pc->AddTrack(audio_track);
    }
    
    if (enable_video) {
        auto capture = IVideoCapture::Create();
        auto renderer = IVideoRenderer::Create();
        
        // Configure capture resolution
        if (capture) {
            capture->SetResolution(width, height);
            capture->SetFrameRate(fps);
        }
        
        auto video_track = std::make_shared<VideoTrack>(
            2, "video_track", 1002,
            std::move(capture), std::move(renderer)
        );
        pc->AddTrack(video_track);
    }
    
    // Start connection
    std::cout << "[4/5] 启动通话..." << std::endl;
    g_running = true;
    
    if (!pc->Start()) {
        std::cerr << "Failed to start PeerConnection\n";
        return 1;
    }
    
    // Wait for call to end
    std::cout << "\n>>> 通话中，按Enter结束通话 <<<\n" << std::endl;
    
    // Print stats periodically
    std::thread stats_thread([&pc, &enable_audio, &enable_video]() {
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            if (!g_running) break;
            
            std::cout << "[Stats] ";
            auto state = pc->GetState();
            switch (state) {
                case PeerConnectionState::kConnected:
                    std::cout << "Connected";
                    break;
                case PeerConnectionState::kConnecting:
                    std::cout << "Connecting...";
                    break;
                default:
                    std::cout << "State: " << static_cast<int>(state);
                    break;
            }
            std::cout << std::endl;
        }
    });
    
    // Wait for user input
    std::cin.get();
    
    // Stop
    std::cout << "\n[5/5] 停止通话..." << std::endl;
    g_running = false;
    
    pc->Stop();
    
    // Wait for stats thread
    if (stats_thread.joinable()) {
        stats_thread.join();
    }
    
    std::cout << "通话已结束.\n";
    
    return 0;
}
