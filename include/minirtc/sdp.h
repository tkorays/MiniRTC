/**
 * @file sdp.h
 * @brief SDP (Session Description Protocol) for MiniRTC
 */

#ifndef MINIRTC_SDP_H_
#define MINIRTC_SDP_H_

#include <string>
#include <vector>
#include <memory>
#include "minirtc/ice.h"

namespace minirtc {

// SDP中的媒体类型
enum class SdpMediaType {
    kAudio,
    kVideo,
    kApplication,
    kUnknown
};

// SDP媒体描述
struct SdpMediaDescription {
    SdpMediaType type = SdpMediaType::kUnknown;
    uint16_t port = 0;
    std::string protocol = "RTP/AVP";
    std::string codec_name;
    uint8_t payload_type = 0;
    uint32_t clock_rate = 0;
    uint8_t channels = 0;
    std::vector<std::string> fmtp;
};

// SDP会话描述
struct SessionDescription {
    // 会话信息
    std::string version = "0";
    std::string origin = "- 0 0 IN IP4 127.0.0.1";
    std::string session_name = "-";
    std::string connection_addr = "127.0.0.1";
    
    // 媒体描述
    std::vector<SdpMediaDescription> media_descriptions;
    
    // ICE信息
    std::vector<IceCandidate> ice_candidates;
    std::string ice_ufrag;
    std::string ice_pwd;
    
    // 生成/解析方向
    bool is_offer = false;
};

// SDP解析器/生成器
class SdpParser {
public:
    // 生成本地SDP (offer)
    static std::string GenerateOffer(
        const std::vector<IceCandidate>& candidates,
        bool enable_audio = true,
        bool enable_video = true,
        uint16_t audio_port = 10000,
        uint16_t video_port = 10002);
    
    // 解析SDP
    static bool Parse(const std::string& sdp, SessionDescription* desc);
    
    // 生成answer SDP
    static std::string GenerateAnswer(
        const SessionDescription& remote_offer,
        const std::vector<IceCandidate>& local_candidates,
        bool enable_audio = true,
        bool enable_video = true,
        uint16_t audio_port = 10000,
        uint16_t video_port = 10002);
    
    // 将SessionDescription转换为SDP字符串
    static std::string ToString(const SessionDescription& desc);
    
    // ICE候选转SDP行
    static std::string IceCandidateToSdpLine(const IceCandidate& candidate, uint16_t mline_index);
    
    // 从SDP行解析ICE候选
    static bool ParseIceCandidateLine(const std::string& line, IceCandidate* candidate);
};

}  // namespace minirtc

#endif
