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
#include <vector>
#include <functional>

// Platform-specific socket includes
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

#include "minirtc/peer_connection.h"
#include "minirtc/capture_render.h"
#include "minirtc/stream_track.h"
#include "minirtc/transport/rtp_packet.h"
#include "minirtc/stats.h"

using namespace minirtc;

// ============================================================================
// Global state
// ============================================================================

std::atomic<bool> g_running{false};

// ============================================================================
// UDP Loopback Manager - 真正的UDP本地环回 (使用原生socket)
// ============================================================================

class UdpLoopbackManager {
public:
    static constexpr uint16_t kSendPort = 10000;
    static constexpr uint16_t kRecvPort = 10001;
    static constexpr const char* kLoopbackAddr = "127.0.0.1";

    UdpLoopbackManager() : send_fd_(-1), recv_fd_(-1), running_(false) {}

    ~UdpLoopbackManager() { Stop(); }

    bool Start() {
        if (running_) return true;

#ifdef _WIN32
        WSADATA wsa_data;
        static bool initialized = false;
        if (!initialized) {
            WSAStartup(MAKEWORD(2, 2), &wsa_data);
            initialized = true;
        }
#endif

        // 创建发送socket
        send_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (send_fd_ < 0) {
            std::cerr << "Failed to create send socket" << std::endl;
            return false;
        }

        // 绑定发送端口
        struct sockaddr_in send_addr;
        std::memset(&send_addr, 0, sizeof(send_addr));
        send_addr.sin_family = AF_INET;
        send_addr.sin_addr.s_addr = INADDR_ANY;
        send_addr.sin_port = htons(kSendPort);

        if (bind(send_fd_, (struct sockaddr*)&send_addr, sizeof(send_addr)) < 0) {
            std::cerr << "Failed to bind send socket: " << strerror(errno) << std::endl;
            close(send_fd_);
            send_fd_ = -1;
            return false;
        }

        std::cout << "[UDP Loopback] 发送端口: " << kSendPort << std::endl;

        // 创建接收socket
        recv_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (recv_fd_ < 0) {
            std::cerr << "Failed to create recv socket" << std::endl;
            close(send_fd_);
            send_fd_ = -1;
            return false;
        }

        // 绑定接收端口
        struct sockaddr_in recv_addr;
        std::memset(&recv_addr, 0, sizeof(recv_addr));
        recv_addr.sin_family = AF_INET;
        recv_addr.sin_addr.s_addr = INADDR_ANY;
        recv_addr.sin_port = htons(kRecvPort);

        if (bind(recv_fd_, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0) {
            std::cerr << "Failed to bind recv socket: " << strerror(errno) << std::endl;
            close(send_fd_);
            close(recv_fd_);
            send_fd_ = -1;
            recv_fd_ = -1;
            return false;
        }

        // 设置为非阻塞模式
#ifndef _WIN32
        int flags = fcntl(recv_fd_, F_GETFL, 0);
        fcntl(recv_fd_, F_SETFL, flags | O_NONBLOCK);
#else
        u_long mode = 1;
        ioctlsocket(recv_fd_, FIONBIO, &mode);
#endif

        std::cout << "[UDP Loopback] 接收端口: " << kRecvPort << std::endl;

        running_ = true;
        recv_thread_ = std::thread(&UdpLoopbackManager::RecvThread, this);

        return true;
    }

    void Stop() {
        if (!running_) return;
        running_ = false;

        if (recv_thread_.joinable()) {
            recv_thread_.join();
        }

        if (send_fd_ >= 0) {
            close(send_fd_);
            send_fd_ = -1;
        }

        if (recv_fd_ >= 0) {
            close(recv_fd_);
            recv_fd_ = -1;
        }
    }

    // 发送RTP包到环回地址
    bool SendRtpPacket(std::shared_ptr<RtpPacket> packet) {
        if (send_fd_ < 0 || !packet) return false;

        auto view = packet->GetView();
        if (view.first == nullptr || view.second == 0) {
            return false;
        }

        struct sockaddr_in to_addr;
        std::memset(&to_addr, 0, sizeof(to_addr));
        to_addr.sin_family = AF_INET;
        to_addr.sin_addr.s_addr = inet_addr(kLoopbackAddr);
        to_addr.sin_port = htons(kRecvPort);

        int sent = sendto(send_fd_, reinterpret_cast<const char*>(view.first), view.second, 0,
                          (struct sockaddr*)&to_addr, sizeof(to_addr));
        return sent >= 0;
    }

    // 设置接收回调
    void SetRecvCallback(std::function<void(std::shared_ptr<RtpPacket>)> callback) {
        recv_callback_ = callback;
    }
    
    // 直接发送RTP包（不经过回调）
    bool SendRtpDirect(const uint8_t* data, size_t size) {
        if (send_fd_ < 0 || !data || size == 0) return false;
        
        struct sockaddr_in to_addr;
        std::memset(&to_addr, 0, sizeof(to_addr));
        to_addr.sin_family = AF_INET;
        to_addr.sin_addr.s_addr = inet_addr(kLoopbackAddr);
        to_addr.sin_port = htons(kRecvPort);
        
        int sent = sendto(send_fd_, reinterpret_cast<const char*>(data), size, 0,
                          (struct sockaddr*)&to_addr, sizeof(to_addr));
        return sent >= 0;
    }

private:
    void RecvThread() {
        std::vector<uint8_t> buffer(2048);
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);

        while (running_) {
            int received = recvfrom(recv_fd_, reinterpret_cast<char*>(buffer.data()), 
                                    buffer.size(), 0,
                                    (struct sockaddr*)&from_addr, &from_len);

            if (received > 0) {
                // 解析RTP包
                auto packet = CreateRtpPacket();
                int deser_result = packet->Deserialize(buffer.data(), received);
                if (deser_result != 0) {
                    continue;
                }
                
                // 调用回调
                if (recv_callback_) {
                    recv_callback_(packet);
                }
            } else {
#ifndef _WIN32
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
#else
                if (WSAGetLastError() == WSAEWOULDBLOCK) {
#endif
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        }
    }

    int send_fd_;
    int recv_fd_;
    std::thread recv_thread_;
    std::atomic<bool> running_;
    std::function<void(std::shared_ptr<RtpPacket>)> recv_callback_;
};

// 全局UDP环回管理器
std::unique_ptr<UdpLoopbackManager> g_udp_loopback;

// ============================================================================
// Stats printing utility
// ============================================================================

void PrintStats(std::shared_ptr<IPeerConnection> pc) {
    auto report = pc->GetStats();
    if (!report) {
        return;
    }
    
    std::cout << "\n";
    std::cout << "============================================\n";
    std::cout << "       MiniRTC 通话统计报告\n";
    std::cout << "============================================\n";
    
    // Session info
    std::string state_str = "unknown";
    if (report->peer_connection_stats) {
        state_str = report->peer_connection_stats->state;
    }
    
    double duration_sec = report->peer_connection_stats 
        ? report->peer_connection_stats->session_duration_ms / 1000.0 
        : 0.0;
    std::cout << "会话状态: " << state_str << "\n";
    std::cout << "持续时间: " << duration_sec << " 秒\n\n";
    
    // Audio stats
    if (!report->audio_sender_stats.empty()) {
        const auto& audio = report->audio_sender_stats[0];
        std::cout << "----------------[ 音频发送 ]----------------\n";
        std::cout << "  发送包数:    " << audio.packets_sent << "\n";
        std::cout << "  发送字节:    " << (audio.bytes_sent / 1024.0 / 1024.0) << " MB\n";
        std::cout << "  编码帧数:    " << audio.frames_encoded << "\n";
        if (audio.encode_time_ms > 0) {
            std::cout << "  编码耗时:    " << audio.encode_time_ms << " ms/帧\n";
        }
        std::cout << "  采样率:      " << audio.sample_rate << " Hz\n";
        std::cout << "  声道数:      " << audio.channels << "\n";
        if (audio.round_trip_time_ms > 0) {
            std::cout << "  RTT:         " << audio.round_trip_time_ms << " ms\n";
        }
        std::cout << "\n";
    }
    
    // Video stats
    if (!report->video_sender_stats.empty()) {
        const auto& video = report->video_sender_stats[0];
        std::cout << "----------------[ 视频发送 ]----------------\n";
        std::cout << "  发送包数:    " << video.packets_sent << "\n";
        std::cout << "  发送字节:    " << (video.bytes_sent / 1024.0 / 1024.0) << " MB\n";
        std::cout << "  编码帧数:    " << video.frames_encoded << "\n";
        std::cout << "  I帧数:       " << video.key_frames_encoded << "\n";
        if (video.encode_time_ms > 0) {
            std::cout << "  编码耗时:    " << video.encode_time_ms << " ms/帧\n";
        }
        std::cout << "  分辨率:      " << video.frame_width << " x " << video.frame_height << "\n";
        if (video.round_trip_time_ms > 0) {
            std::cout << "  RTT:         " << video.round_trip_time_ms << " ms\n";
        }
        std::cout << "\n";
    }
    
    // Transport stats
    if (report->transport_stats) {
        const auto& tp = *report->transport_stats;
        std::cout << "----------------[ 传输层 ]-----------------\n";
        std::cout << "  RTP发送包:   " << tp.packets_sent << "\n";
        std::cout << "  RTP接收包:   " << tp.packets_received << "\n";
        std::cout << "  RTP发送字节: " << (tp.bytes_sent / 1024.0 / 1024.0) << " MB\n";
        std::cout << "  RTP接收字节: " << (tp.bytes_received / 1024.0 / 1024.0) << " MB\n";
        
        // Calculate bitrate
        if (duration_sec > 0) {
            double send_bps = (tp.bytes_sent * 8.0) / duration_sec / 1000.0;  // kbps
            double recv_bps = (tp.bytes_received * 8.0) / duration_sec / 1000.0;  // kbps
            std::cout << "  发送码率:    " << send_bps << " kbps\n";
            std::cout << "  接收码率:    " << recv_bps << " kbps\n";
        }
        std::cout << "\n";
    }
    
    // Packet loss
    if (report->transport_stats) {
        const auto& tp = *report->transport_stats;
        uint64_t total_packets = tp.packets_sent + tp.packets_received;
        if (total_packets > 0) {
            double loss_rate = 100.0 * (double)tp.packets_received / total_packets;
            std::cout << "----------------[ 丢包率 ]-----------------\n";
            std::cout << "  整体丢包率: " << (100.0 - loss_rate) << "%\n";
        }
    }
    
    std::cout << "============================================\n";
}

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
    using SendRtpCallback = std::function<bool(std::shared_ptr<RtpPacket>)>;
    using UdpLoopbackPtr = UdpLoopbackManager*;
    
    AudioTrack(uint32_t id, const std::string& name, uint32_t ssrc,
               std::unique_ptr<IAudioCapture> capture,
               std::unique_ptr<IAudioPlayer> player,
               SendRtpCallback send_rtp_callback = nullptr,
               UdpLoopbackPtr udp_loopback = nullptr)
        : id_(id), name_(name), ssrc_(ssrc), 
          capture_(std::move(capture)), player_(std::move(player)),
          send_rtp_callback_(std::move(send_rtp_callback)),
          udp_loopback_(udp_loopback),
          running_(false), rtp_seq_(0), rtp_timestamp_(0) {
        
        // Initialize capture
        if (capture_) {
            capture_->Initialize();
            AudioCaptureParam param;
            param.sample_rate = 48000;
            param.channels = 1;  // 单声道，这样 payload = 320 * 1 * 2 = 640 字节 < 1460
            param.frames_per_buffer = 320;
            capture_->SetParam(param);
        }
        
        // Initialize player
        if (player_) {
            player_->Initialize();
            AudioPlayParam param;
            param.sample_rate = 48000;
            param.channels = 1;  // 单声道以匹配capture
            player_->SetParam(param);
        }
        
        // Set audio info for stats
        stats_.sample_rate = 48000;
        stats_.channels = 1;
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
            running_ = true;  // 先设置为true，确保回调可以发送RTP
            
            // Start capture
            if (capture_) {
                capture_->StartCapture(this);
            }
            // Start player
            if (player_) {
                player_->StartPlay(this);
            }
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
        stats_.bytes_sent += packet->GetPayloadSize();
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
    
    // AudioCaptureObserver
    void OnFrameCaptured(const AudioFrame& frame) override {
        // In loopback mode, play the captured frame
        if (player_ && running_) {
            player_->PlayFrame(frame);
        }
        
        // 如果有UDP环回，则发送RTP包
        if (udp_loopback_ && running_) {
            static int frame_count = 0;
            frame_count++;
            
            // 手动构建RTP包
            uint8_t rtp_buffer[2048];
            // RTP header (12 bytes)
            rtp_buffer[0] = 0x80;  // V=2, P=0, X=0, CC=0
            rtp_buffer[1] = 111;    // PT=111 (Opus)
            rtp_buffer[2] = (rtp_seq_) >> 8;
            rtp_buffer[3] = (rtp_seq_) & 0xFF;
            uint32_t ts = rtp_timestamp_ - frame.samples_per_channel;
            rtp_buffer[4] = ts >> 24;
            rtp_buffer[5] = (ts >> 16) & 0xFF;
            rtp_buffer[6] = (ts >> 8) & 0xFF;
            rtp_buffer[7] = ts & 0xFF;
            rtp_buffer[8] = (ssrc_) >> 24;
            rtp_buffer[9] = ((ssrc_) >> 16) & 0xFF;
            rtp_buffer[10] = ((ssrc_) >> 8) & 0xFF;
            rtp_buffer[11] = (ssrc_) & 0xFF;
            
            // 复制payload
            size_t payload_size = std::min(frame.data.size(), static_cast<size_t>(1400));
            std::copy(frame.data.begin(), frame.data.begin() + payload_size, rtp_buffer + 12);
            
            // 发送RTP包
            if (udp_loopback_->SendRtpDirect(rtp_buffer, 12 + payload_size)) {
                rtp_seq_++;
                rtp_timestamp_ += frame.samples_per_channel;
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.rtp_sent++;
                stats_.bytes_sent += 12 + payload_size;
            }
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
    SendRtpCallback send_rtp_callback_;
    UdpLoopbackManager* udp_loopback_;
    bool running_;
    mutable std::mutex mutex_;
    TrackStats stats_;
    uint16_t rtp_seq_;
    uint32_t rtp_timestamp_;
};

class VideoTrack : public ITrack, public VideoCaptureObserver, public VideoRenderObserver {
public:
    using SendRtpCallback = std::function<bool(std::shared_ptr<RtpPacket>)>;
    using UdpLoopbackPtr = UdpLoopbackManager*;
    
    VideoTrack(uint32_t id, const std::string& name, uint32_t ssrc,
               std::unique_ptr<IVideoCapture> capture,
               std::unique_ptr<IVideoRenderer> renderer,
               SendRtpCallback send_rtp_callback = nullptr,
               UdpLoopbackPtr udp_loopback = nullptr)
        : id_(id), name_(name), ssrc_(ssrc),
          capture_(std::move(capture)), renderer_(std::move(renderer)),
          send_rtp_callback_(std::move(send_rtp_callback)),
          udp_loopback_(udp_loopback),
          running_(false), rtp_seq_(0), rtp_timestamp_(0) {
        
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
        
        // Set video info for stats
        stats_.frame_width = 640;
        stats_.frame_height = 480;
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
            running_ = true;  // 先设置为true
            
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
        stats_.bytes_sent += packet->GetPayloadSize();
    }
    
    void OnRtpPacketReceived(std::shared_ptr<RtpPacket> packet) override {
        if (!packet) return;
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.rtp_received++;
        stats_.bytes_received += packet->GetPayloadSize();
        
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
        
        // 如果有UDP环回，则发送RTP包
        if (udp_loopback_ && running_) {
            // 手动构建RTP包
            uint8_t rtp_buffer[2048];
            // RTP header (12 bytes)
            rtp_buffer[0] = 0x80;  // V=2, P=0, X=0, CC=0
            rtp_buffer[1] = 96;     // PT=96 (H264)
            rtp_buffer[2] = (rtp_seq_) >> 8;
            rtp_buffer[3] = (rtp_seq_) & 0xFF;
            rtp_buffer[4] = (rtp_timestamp_) >> 24;
            rtp_buffer[5] = ((rtp_timestamp_) >> 16) & 0xFF;
            rtp_buffer[6] = ((rtp_timestamp_) >> 8) & 0xFF;
            rtp_buffer[7] = (rtp_timestamp_) & 0xFF;
            rtp_buffer[8] = (ssrc_) >> 24;
            rtp_buffer[9] = ((ssrc_) >> 16) & 0xFF;
            rtp_buffer[10] = ((ssrc_) >> 8) & 0xFF;
            rtp_buffer[11] = (ssrc_) & 0xFF;
            
            // 设置payload (从YUV数据构建简单的payload)
            size_t frame_size = frame.GetBufferSize();
            // 限制payload大小
            size_t payload_size = std::min(frame_size, static_cast<size_t>(1400));
            if (frame.data_y && payload_size > 0) {
                std::copy(frame.data_y, frame.data_y + payload_size, rtp_buffer + 12);
            }
            
            // 发送RTP包
            if (udp_loopback_->SendRtpDirect(rtp_buffer, 12 + payload_size)) {
                rtp_seq_++;
                rtp_timestamp_ += 3000;  // 30fps, 90kHz/30 = 3000
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.rtp_sent++;
                stats_.bytes_sent += 12 + payload_size;
            }
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
    SendRtpCallback send_rtp_callback_;
    UdpLoopbackManager* udp_loopback_;
    bool running_;
    mutable std::mutex mutex_;
    TrackStats stats_;
    uint16_t rtp_seq_;
    uint32_t rtp_timestamp_;
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
    
    // 保存track的shared_ptr以便设置回调
    std::shared_ptr<AudioTrack> audio_track_ptr;
    std::shared_ptr<VideoTrack> video_track_ptr;
    
    // 如果是loopback模式，启动UDP环回
    if (mode == "loopback") {
        std::cout << "[UDP] 启动UDP本地环回..." << std::endl;
        g_udp_loopback = std::make_unique<UdpLoopbackManager>();
        if (!g_udp_loopback->Start()) {
            std::cerr << "Failed to start UDP loopback\n";
            return 1;
        }
        std::cout << "[UDP] UDP环回已启动 (发送端口: " << UdpLoopbackManager::kSendPort 
                  << ", 接收端口: " << UdpLoopbackManager::kRecvPort << ")" << std::endl;
    }
    
    if (enable_audio) {
        auto capture = IAudioCapture::Create();
        auto player = IAudioPlayer::Create();
        
        audio_track_ptr = std::make_shared<AudioTrack>(
            1, "audio_track", 1001, 
            std::move(capture), std::move(player),
            nullptr,  // send_rtp_callback (unused)
            g_udp_loopback.get()  // udp_loopback
        );
        pc->AddTrack(audio_track_ptr);
    }
    
    if (enable_video) {
        auto capture = IVideoCapture::Create();
        auto renderer = IVideoRenderer::Create();
        
        // Configure capture resolution
        if (capture) {
            capture->SetResolution(width, height);
            capture->SetFrameRate(fps);
        }
        
        video_track_ptr = std::make_shared<VideoTrack>(
            2, "video_track", 1002,
            std::move(capture), std::move(renderer),
            nullptr,  // send_rtp_callback (unused)
            g_udp_loopback.get()  // udp_loopback
        );
        pc->AddTrack(video_track_ptr);
    }
    
    // 设置UDP接收回调
    if (mode == "loopback" && g_udp_loopback) {
        g_udp_loopback->SetRecvCallback([&audio_track_ptr, &video_track_ptr](std::shared_ptr<RtpPacket> packet) {
            if (!packet) return;
            
            // 根据SSRC判断是音频还是视频
            uint32_t ssrc = packet->GetSsrc();
            if (audio_track_ptr && ssrc == 1001) {
                audio_track_ptr->OnRtpPacketReceived(packet);
            } else if (video_track_ptr && ssrc == 1002) {
                video_track_ptr->OnRtpPacketReceived(packet);
            }
        });
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
    
    // Wait for user input or auto-timeout in loopback mode
    if (mode == "loopback") {
        // 在loopback模式下，等待5秒让RTP包传输
        std::cout << "\n>>> Loopback模式运行5秒后自动结束 <<<\n" << std::endl;
        
        // 每秒打印一次状态
        for (int i = 0; i < 5; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "[Loopback] 运行中... " << (i+1) << "/5秒" << std::endl;
        }
    } else {
        std::cin.get();
    }
    
    // Stop
    std::cout << "\n[5/5] 停止通话..." << std::endl;
    g_running = false;
    
    pc->Stop();
    
    // 停止UDP环回
    if (g_udp_loopback) {
        g_udp_loopback->Stop();
        g_udp_loopback.reset();
    }
    
    // Print stats before exit
    PrintStats(pc);
    
    // 在loopback模式下，从track获取额外统计
    if (mode == "loopback" && (audio_track_ptr || video_track_ptr)) {
        std::cout << "---------[ Loopback Track统计 ]----------\n";
        if (audio_track_ptr) {
            auto audio_stats = audio_track_ptr->GetStats();
            std::cout << "  Audio RTP发送: " << audio_stats.rtp_sent << " 包, " 
                      << (audio_stats.bytes_sent / 1024.0) << " KB\n";
            std::cout << "  Audio RTP接收: " << audio_stats.rtp_received << " 包, " 
                      << (audio_stats.bytes_received / 1024.0) << " KB\n";
        }
        if (video_track_ptr) {
            auto video_stats = video_track_ptr->GetStats();
            std::cout << "  Video RTP发送: " << video_stats.rtp_sent << " 包, " 
                      << (video_stats.bytes_sent / 1024.0) << " KB\n";
            std::cout << "  Video RTP接收: " << video_stats.rtp_received << " 包, " 
                      << (video_stats.bytes_received / 1024.0) << " KB\n";
        }
    }
    
    // Wait for stats thread
    if (stats_thread.joinable()) {
        stats_thread.join();
    }
    
    std::cout << "通话已结束.\n";
    
    return 0;
}
