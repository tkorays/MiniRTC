/**
 * @file sdp.cc
 * @brief SDP implementation for MiniRTC
 */

#include "minirtc/sdp.h"

#include <sstream>
#include <algorithm>
#include <regex>
#include <cstring>

namespace minirtc {

// ============================================================================
// SDP Parser Implementation
// ============================================================================

std::string SdpParser::GenerateOffer(
    const std::vector<IceCandidate>& candidates,
    bool enable_audio,
    bool enable_video,
    uint16_t audio_port,
    uint16_t video_port) {
    
    SessionDescription desc;
    desc.is_offer = true;
    desc.version = "0";
    desc.origin = "- 0 0 IN IP4 127.0.0.1";
    desc.session_name = "MiniRTC Demo";
    desc.ice_ufrag = "abcd";
    desc.ice_pwd = "efghijklmnopqrstuvwx";
    
    // 添加音频媒体描述
    if (enable_audio) {
        SdpMediaDescription audio;
        audio.type = SdpMediaType::kAudio;
        audio.port = audio_port;
        audio.protocol = "RTP/AVP";
        audio.payload_type = 111;  // Opus
        audio.codec_name = "opus";
        audio.clock_rate = 48000;
        audio.channels = 2;
        desc.media_descriptions.push_back(audio);
    }
    
    // 添加视频媒体描述
    if (enable_video) {
        SdpMediaDescription video;
        video.type = SdpMediaType::kVideo;
        video.port = video_port;
        video.protocol = "RTP/AVP";
        video.payload_type = 96;  // H264
        video.codec_name = "H264";
        video.clock_rate = 90000;
        desc.media_descriptions.push_back(video);
    }
    
    // 获取第一个非localhost的IP作为连接地址
    for (const auto& c : candidates) {
        if (!c.host_addr.empty() && c.host_addr.find("127.") != 0) {
            desc.connection_addr = c.host_addr;
            break;
        }
    }
    
    // ICE候选
    desc.ice_candidates = candidates;
    
    return ToString(desc);
}

std::string SdpParser::ToString(const SessionDescription& desc) {
    std::ostringstream sdp;
    
    // 会话描述
    sdp << "v=" << desc.version << "\r\n";
    sdp << "o=" << desc.origin << "\r\n";
    sdp << "s=" << desc.session_name << "\r\n";
    sdp << "c=IN IP4 " << desc.connection_addr << "\r\n";
    sdp << "t=0 0\r\n";
    
    // ICE信息
    if (!desc.ice_ufrag.empty()) {
        sdp << "a=ice-ufrag:" << desc.ice_ufrag << "\r\n";
    }
    if (!desc.ice_pwd.empty()) {
        sdp << "a=ice-pwd:" << desc.ice_pwd << "\r\n";
    }
    
    // 媒体描述
    for (size_t i = 0; i < desc.media_descriptions.size(); ++i) {
        const auto& media = desc.media_descriptions[i];
        
        std::string media_type;
        switch (media.type) {
            case SdpMediaType::kAudio: media_type = "audio"; break;
            case SdpMediaType::kVideo: media_type = "video"; break;
            case SdpMediaType::kApplication: media_type = "application"; break;
            default: media_type = "unknown"; break;
        }
        
        sdp << "m=" << media_type << " " << media.port << " " << media.protocol 
            << " " << static_cast<int>(media.payload_type) << "\r\n";
        
        // rtpmap
        if (!media.codec_name.empty()) {
            sdp << "a=rtpmap:" << static_cast<int>(media.payload_type) << " " 
                << media.codec_name << "/" << media.clock_rate;
            if (media.channels > 0 && media.type == SdpMediaType::kAudio) {
                sdp << "/" << static_cast<int>(media.channels);
            }
            sdp << "\r\n";
        }
        
        // fmtp
        for (const auto& fmtp : media.fmtp) {
            sdp << "a=fmtp:" << static_cast<int>(media.payload_type) << " " << fmtp << "\r\n";
        }
    }
    
    // ICE候选
    for (size_t mline_index = 0; mline_index < desc.ice_candidates.size(); ++mline_index) {
        const auto& candidate = desc.ice_candidates[mline_index];
        sdp << IceCandidateToSdpLine(candidate, mline_index) << "\r\n";
    }
    
    return sdp.str();
}

bool SdpParser::Parse(const std::string& sdp, SessionDescription* desc) {
    if (!desc) return false;
    
    std::istringstream iss(sdp);
    std::string line;
    
    size_t current_mline = 0;
    SdpMediaDescription* current_media = nullptr;
    
    while (std::getline(iss, line)) {
        // 去掉末尾的\r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line.empty()) continue;
        
        // 解析会话描述行
        if (line[0] == 'v' && line.length() >= 2 && line[1] == '=') {
            desc->version = line.substr(2);
        } else if (line[0] == 'o' && line.length() >= 2 && line[1] == '=') {
            desc->origin = line.substr(2);
        } else if (line[0] == 's' && line.length() >= 2 && line[1] == '=') {
            desc->session_name = line.substr(2);
        } else if (line[0] == 'c' && line.length() >= 2 && line[1] == '=') {
            // c=IN IP4 address
            size_t pos = line.find("IP4 ");
            if (pos != std::string::npos) {
                desc->connection_addr = line.substr(pos + 4);
            }
        } else if (line[0] == 't' && line.length() >= 2 && line[1] == '=') {
            // timing (ignored)
        } else if (line[0] == 'a' && line.length() >= 2 && line[1] == '=') {
            std::string attr = line.substr(2);
            
            // ICE属性
            if (attr.find("ice-ufrag:") == 0) {
                desc->ice_ufrag = attr.substr(11);
            } else if (attr.find("ice-pwd:") == 0) {
                desc->ice_pwd = attr.substr(9);
            } else if (attr.find("candidate:") == 0) {
                // ICE候选
                IceCandidate candidate;
                if (ParseIceCandidateLine(line, &candidate)) {
                    desc->ice_candidates.push_back(candidate);
                }
            } else if (attr.find("rtpmap:") == 0) {
                // rtpmap:PT codec/clock[/channels]
                if (current_media) {
                    size_t space_pos = attr.find(' ');
                    if (space_pos != std::string::npos) {
                        std::string pt_str = attr.substr(7, space_pos - 7);
                        std::string codec_info = attr.substr(space_pos + 1);
                        
                        current_media->payload_type = static_cast<uint8_t>(std::stoi(pt_str));
                        
                        size_t slash_pos = codec_info.find('/');
                        if (slash_pos != std::string::npos) {
                            current_media->codec_name = codec_info.substr(0, slash_pos);
                            std::string rest = codec_info.substr(slash_pos + 1);
                            slash_pos = rest.find('/');
                            if (slash_pos != std::string::npos) {
                                current_media->clock_rate = std::stoi(rest.substr(0, slash_pos));
                                current_media->channels = static_cast<uint8_t>(std::stoi(rest.substr(slash_pos + 1)));
                            } else {
                                current_media->clock_rate = std::stoi(rest);
                            }
                        }
                    }
                }
            }
        } else if (line[0] == 'm' && line.length() >= 2 && line[1] == '=') {
            // m=media port proto payload-types
            std::istringstream mss(line.substr(2));
            std::string media_type_str;
            uint16_t port;
            std::string proto;
            int pt;
            
            mss >> media_type_str >> port >> proto >> pt;
            
            SdpMediaDescription media;
            media.port = port;
            media.protocol = proto;
            media.payload_type = static_cast<uint8_t>(pt);
            
            if (media_type_str == "audio") {
                media.type = SdpMediaType::kAudio;
            } else if (media_type_str == "video") {
                media.type = SdpMediaType::kVideo;
            } else if (media_type_str == "application") {
                media.type = SdpMediaType::kApplication;
            }
            
            desc->media_descriptions.push_back(media);
            current_media = &desc->media_descriptions.back();
            current_mline++;
        }
    }
    
    return true;
}

std::string SdpParser::GenerateAnswer(
    const SessionDescription& remote_offer,
    const std::vector<IceCandidate>& local_candidates,
    bool enable_audio,
    bool enable_video,
    uint16_t audio_port,
    uint16_t video_port) {
    
    SessionDescription desc;
    desc.is_offer = false;
    desc.version = "0";
    desc.origin = "- 0 0 IN IP4 127.0.0.1";
    desc.session_name = "MiniRTC Demo";
    desc.ice_ufrag = "wxyz";
    desc.ice_pwd = "qrstuvwxyz12345678";
    
    // 使用远端的连接地址
    desc.connection_addr = remote_offer.connection_addr;
    
    // 添加音频媒体描述
    bool has_audio = false;
    for (const auto& media : remote_offer.media_descriptions) {
        if (media.type == SdpMediaType::kAudio && enable_audio) {
            SdpMediaDescription audio;
            audio.type = SdpMediaType::kAudio;
            audio.port = audio_port;
            audio.protocol = "RTP/AVP";
            audio.payload_type = media.payload_type;
            audio.codec_name = media.codec_name;
            audio.clock_rate = media.clock_rate;
            audio.channels = media.channels;
            desc.media_descriptions.push_back(audio);
            has_audio = true;
            break;
        }
    }
    
    // 添加视频媒体描述
    bool has_video = false;
    for (const auto& media : remote_offer.media_descriptions) {
        if (media.type == SdpMediaType::kVideo && enable_video) {
            SdpMediaDescription video;
            video.type = SdpMediaType::kVideo;
            video.port = video_port;
            video.protocol = "RTP/AVP";
            video.payload_type = media.payload_type;
            video.codec_name = media.codec_name;
            video.clock_rate = media.clock_rate;
            desc.media_descriptions.push_back(video);
            has_video = true;
            break;
        }
    }
    
    // ICE候选
    desc.ice_candidates = local_candidates;
    
    return ToString(desc);
}

std::string SdpParser::IceCandidateToSdpLine(const IceCandidate& candidate, uint16_t mline_index) {
    std::ostringstream line;
    line << "a=candidate:" << candidate.foundation << " " << candidate.component_id << " "
         << "UDP " << candidate.priority << " " 
         << candidate.host_addr << " " << candidate.port << " "
         << "typ host";
    return line.str();
}

bool SdpParser::ParseIceCandidateLine(const std::string& line, IceCandidate* candidate) {
    if (!candidate || line.find("a=candidate:") != 0) {
        return false;
    }
    
    // a=candidate:1 1 UDP 2130706431 192.168.1.100 10000 typ host
    std::string rest = line.substr(12);
    std::istringstream iss(rest);
    
    std::string foundation;
    uint32_t component_id;
    std::string protocol;
    uint32_t priority;
    std::string host_addr;
    uint16_t port;
    std::string typ;
    std::string type_str;
    
    iss >> foundation >> component_id >> protocol >> priority 
        >> host_addr >> port >> typ >> type_str;
    
    if (typ != "typ" || type_str != "host") {
        return false;
    }
    
    candidate->foundation = std::stoul(foundation);
    candidate->component_id = component_id;
    candidate->protocol = (protocol == "TCP") ? IceProtocol::kTcp : IceProtocol::kUdp;
    candidate->priority = priority;
    candidate->host_addr = host_addr;
    candidate->port = port;
    candidate->type = IceCandidateType::kHost;
    
    return true;
}

}  // namespace minirtc
