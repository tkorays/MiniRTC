/**
 * @file ice.cc
 * @brief MiniRTC ICE (STUN client) implementation
 */

#include "minirtc/ice.h"

#include <cstring>
#include <chrono>
#include <thread>
#include <random>
#include <sstream>
#include <iomanip>
#include <netdb.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <cstdio>
#endif

namespace minirtc {

// ============================================================================
// STUN Constants
// ============================================================================

static const uint32_t kStunMagicCookie = 0x2112A442;

// STUN message header size
static const size_t kStunHeaderSize = 20;

// ============================================================================
// Platform Helpers
// ============================================================================

#ifdef _WIN32
using SocketType = SOCKET;
const SocketType kInvalidSocket = INVALID_SOCKET;
inline int CloseSocket(SocketType s) { return closesocket(s); }
inline int GetLastError() { return WSAGetLastError(); }
#else
using SocketType = int;
const SocketType kInvalidSocket = -1;
inline int CloseSocket(SocketType s) { return close(s); }
inline int GetLastError() { return errno; }
#endif

// ============================================================================
// STUN Message
// ============================================================================

struct StunMessage {
    uint16_t msg_type;
    uint16_t msg_length;
    uint32_t magic_cookie;
    uint8_t transaction_id[12];
    
    std::vector<uint8_t> data;
    
    StunMessage() : msg_type(0), msg_length(0), magic_cookie(kStunMagicCookie) {
        memset(transaction_id, 0, sizeof(transaction_id));
    }
};

// ============================================================================
// STUN Attribute
// ============================================================================

struct StunAttribute {
    uint16_t type;
    uint16_t length;
    std::vector<uint8_t> value;
    
    StunAttribute() : type(0), length(0) {}
};

// ============================================================================
// ICE Agent Implementation
// ============================================================================

class IceAgent : public IIceAgent {
public:
    IceAgent() : socket_fd_(kInvalidSocket) {}
    ~IceAgent() override { Close(); }
    
    bool Initialize(const StunConfig& config) override {
        config_ = config;
        return true;
    }
    
    std::vector<IceCandidate> GatherCandidates() override {
        std::vector<IceCandidate> candidates;
        
        // Get local IPs and create host candidates
        auto local_ips = GetLocalIPs();
        
        uint32_t foundation = 0;
        for (const auto& ip : local_ips) {
            IceCandidate cand;
            cand.foundation = ++foundation;
            cand.component_id = 1;
            cand.protocol = IceProtocol::kUdp;
            cand.priority = 126 << 24 | 65536 | (256 - 1);  // IPv4 UDP priority
            cand.host_addr = ip;
            cand.base_addr = ip;
            cand.port = 0;  // Will be assigned when binding
            cand.type = IceCandidateType::kHost;
            cand.transport_addr = ip + ":0";
            candidates.push_back(cand);
        }
        
        return candidates;
    }
    
    bool Bind(const IceCandidate& candidate, IceCandidate* mapped_candidate) override {
        if (!mapped_candidate) {
            return false;
        }
        
        // Try each STUN server
        for (const auto& server : config_.servers) {
            std::string host;
            uint16_t port;
            if (!ParseHostPort(server, &host, &port)) {
                continue;
            }
            
            if (DoBind(host, port, mapped_candidate)) {
                return true;
            }
        }
        
        return false;
    }
    
    static std::vector<std::string> GetLocalIPs() {
        std::vector<std::string> ips;
        
#ifdef _WIN32
        WSADATA wsa_data;
        static bool initialized = false;
        if (!initialized) {
            WSAStartup(MAKEWORD(2, 2), &wsa_data);
            initialized = true;
        }
        
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            struct addrinfo hints, *res;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_DGRAM;
            
            if (getaddrinfo(hostname, NULL, &hints, &res) == 0) {
                for (struct addrinfo* p = res; p != NULL; p = p->ai_next) {
                    char ip_str[INET6_ADDRSTRLEN];
                    if (p->ai_family == AF_INET) {
                        struct sockaddr_in* addr = (struct sockaddr_in*)p->ai_addr;
                        inet_ntop(AF_INET, &addr->sin_addr, ip_str, INET_ADDRSTRLEN);
                        std::string ip(ip_str);
                        // Skip localhost
                        if (ip.find("127.") != 0) {
                            ips.push_back(ip);
                        }
                    } else if (p->ai_family == AF_INET6) {
                        struct sockaddr_in6* addr = (struct sockaddr_in6*)p->ai_addr;
                        inet_ntop(AF_INET6, &addr->sin6_addr, ip_str, INET6_ADDRSTRLEN);
                        std::string ip(ip_str);
                        // Skip link-local and localhost
                        if (ip.find("fe80:") != 0 && ip.find("::1") != 0) {
                            ips.push_back(ip);
                        }
                    }
                }
                freeaddrinfo(res);
            }
        }
#else
        struct ifaddrs* ifaddr = NULL;
        if (getifaddrs(&ifaddr) == 0) {
            for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == NULL) continue;
                
                char ip_str[INET6_ADDRSTRLEN];
                
                if (ifa->ifa_addr->sa_family == AF_INET) {
                    struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
                    inet_ntop(AF_INET, &addr->sin_addr, ip_str, INET_ADDRSTRLEN);
                    std::string ip(ip_str);
                    // Skip localhost
                    if (ip.find("127.") != 0) {
                        ips.push_back(ip);
                    }
                } else if (ifa->ifa_addr->sa_family == AF_INET6) {
                    struct sockaddr_in6* addr = (struct sockaddr_in6*)ifa->ifa_addr;
                    inet_ntop(AF_INET6, &addr->sin6_addr, ip_str, INET6_ADDRSTRLEN);
                    std::string ip(ip_str);
                    // Skip link-local and localhost
                    if (ip.find("fe80:") != 0 && ip.find("::1") != 0) {
                        ips.push_back(ip);
                    }
                }
            }
            freeifaddrs(ifaddr);
        }
#endif
        
        // Remove duplicates
        std::sort(ips.begin(), ips.end());
        ips.erase(std::unique(ips.begin(), ips.end()), ips.end());
        
        return ips;
    }

private:
    void Close() {
        if (socket_fd_ != kInvalidSocket) {
            CloseSocket(socket_fd_);
            socket_fd_ = kInvalidSocket;
        }
    }
    
    bool ParseHostPort(const std::string& hostport, std::string* host, uint16_t* port) {
        auto colon_pos = hostport.rfind(':');
        if (colon_pos == std::string::npos) {
            return false;
        }
        
        *host = hostport.substr(0, colon_pos);
        try {
            *port = static_cast<uint16_t>(std::stoi(hostport.substr(colon_pos + 1)));
        } catch (...) {
            return false;
        }
        return true;
    }
    
    bool DoBind(const std::string& host, uint16_t port, IceCandidate* mapped_candidate) {
#ifdef _WIN32
        WSADATA wsa_data;
        static bool initialized = false;
        if (!initialized) {
            WSAStartup(MAKEWORD(2, 2), &wsa_data);
            initialized = true;
        }
#endif
        
        // Create UDP socket
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ == kInvalidSocket) {
            return false;
        }
        
        // Set timeout
        struct timeval tv;
        tv.tv_sec = config_.timeout_ms / 1000;
        tv.tv_usec = (config_.timeout_ms % 1000) * 1000;
        setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        
        // Resolve STUN server
        struct sockaddr_in stun_addr;
        memset(&stun_addr, 0, sizeof(stun_addr));
        stun_addr.sin_family = AF_INET;
        stun_addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &stun_addr.sin_addr);
        
        // If host is not IP, resolve it
        if (stun_addr.sin_addr.s_addr == INADDR_NONE) {
            struct hostent* he = gethostbyname(host.c_str());
            if (!he) {
                CloseSocket(socket_fd_);
                socket_fd_ = kInvalidSocket;
                return false;
            }
            memcpy(&stun_addr.sin_addr, he->h_addr_list[0], he->h_length);
        }
        
        // Create STUN binding request
        StunMessage request;
        request.msg_type = static_cast<uint16_t>(StunMessageType::kBindingRequest);
        
        // Generate random transaction ID
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for (size_t i = 0; i < 12; ++i) {
            request.transaction_id[i] = static_cast<uint8_t>(dis(gen));
        }
        
        // Build request message
        std::vector<uint8_t> request_data;
        request_data.resize(kStunHeaderSize);
        uint16_t* p = reinterpret_cast<uint16_t*>(request_data.data());
        uint32_t* p32 = reinterpret_cast<uint32_t*>(request_data.data());
        p[0] = htons(request.msg_type);
        p[1] = htons(0);  // length
        p32[1] = htonl(kStunMagicCookie);  // magic cookie at offset 4 (word index 2)
        memcpy(&request_data[4], request.transaction_id, 12);
        
        // Send request with retries
        for (int retry = 0; retry < config_.max_retries; ++retry) {
            ssize_t sent = sendto(socket_fd_, 
                                  request_data.data(), 
                                  request_data.size(), 
                                  0, 
                                  (struct sockaddr*)&stun_addr, 
                                  sizeof(stun_addr));
            if (sent < 0) {
                continue;
            }
            
            // Receive response
            uint8_t response_buf[1024];
            struct sockaddr_in from_addr;
            socklen_t from_len = sizeof(from_addr);
            
            ssize_t recv_len = recvfrom(socket_fd_, 
                                        response_buf, 
                                        sizeof(response_buf), 
                                        0, 
                                        (struct sockaddr*)&from_addr, 
                                        &from_len);
            
            if (recv_len > 0) {
                // Parse response
                StunMessage response;
                if (ParseStunResponse(response_buf, recv_len, &response)) {
                    // Check if it's a success response
                    if (response.msg_type == static_cast<uint16_t>(StunMessageType::kBindingSuccessResponse)) {
                        // Find XOR-MAPPED-ADDRESS
                        std::string mapped_addr;
                        uint16_t mapped_port = 0;
                        if (FindXorMappedAddress(response, &mapped_addr, &mapped_port)) {
                            mapped_candidate->type = IceCandidateType::kSrflx;
                            mapped_candidate->host_addr = mapped_addr;
                            mapped_candidate->transport_addr = mapped_addr + ":" + std::to_string(mapped_port);
                            mapped_candidate->port = mapped_port;
                            CloseSocket(socket_fd_);
                            socket_fd_ = kInvalidSocket;
                            return true;
                        }
                    }
                }
            }
        }
        
        CloseSocket(socket_fd_);
        socket_fd_ = kInvalidSocket;
        return false;
    }
    
    bool ParseStunResponse(const uint8_t* data, size_t len, StunMessage* response) {
        if (len < kStunHeaderSize) {
            return false;
        }
        
        const uint16_t* p = reinterpret_cast<const uint16_t*>(data);
        response->msg_type = ntohs(p[0]);
        response->msg_length = ntohs(p[1]);
        response->magic_cookie = ntohs(p[2]);  // This is actually only 16-bit in header
        // Note: magic cookie in header is 32-bit but only upper 16 bits in p[2]
        response->magic_cookie = kStunMagicCookie;
        
        memcpy(response->transaction_id, data + 4, 12);
        
        // Parse attributes
        size_t offset = kStunHeaderSize;
        while (offset + 4 <= len) {
            uint16_t attr_type = ntohs(*(uint16_t*)(data + offset));
            uint16_t attr_len = ntohs(*(uint16_t*)(data + offset + 2));
            
            if (offset + 4 + attr_len > len) {
                break;
            }
            
            StunAttribute attr;
            attr.type = attr_type;
            attr.length = attr_len;
            attr.value.assign(data + offset + 4, data + offset + 4 + attr_len);
            
            response->data.insert(response->data.end(), data + offset, data + offset + 4 + attr_len);
            
            offset += 4 + attr_len;
            // Padding
            if (attr_len % 4 != 0) {
                offset += 4 - (attr_len % 4);
            }
        }
        
        return true;
    }
    
    bool FindXorMappedAddress(const StunMessage& response, std::string* address, uint16_t* port) {
        // XOR-MAPPED-ADDRESS attribute
        const uint16_t kXorMappedAddress = 0x0020;
        
        size_t offset = 0;
        while (offset + 4 <= response.data.size()) {
            uint16_t attr_type = ntohs(*(uint16_t*)(response.data.data() + offset));
            uint16_t attr_len = ntohs(*(uint16_t*)(response.data.data() + offset + 2));
            
            if (attr_type == kXorMappedAddress && attr_len >= 4) {
                const uint8_t* val = response.data.data() + offset + 4;
                
                // First byte is family (0x01 = IPv4, 0x02 = IPv6)
                uint8_t family = val[0];
                
                if (family == 0x01) {  // IPv4
                    // XOR port with magic cookie
                    uint16_t xored_port = *(uint16_t*)(val + 2);
                    *port = ntohs(xored_port) ^ (kStunMagicCookie >> 16);
                    
                    // XOR address with magic cookie
                    uint32_t xored_addr = *(uint32_t*)(val + 4);
                    uint32_t xored_addr_full = ntohl(xored_addr) ^ kStunMagicCookie;
                    
                    struct in_addr in;
                    in.s_addr = htonl(xored_addr_full);
                    char addr_str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &in, addr_str, INET_ADDRSTRLEN);
                    *address = addr_str;
                    
                    return true;
                } else if (family == 0x02) {  // IPv6
                    // XOR port with magic cookie
                    uint16_t xored_port = *(uint16_t*)(val + 2);
                    *port = ntohs(xored_port) ^ (kStunMagicCookie >> 16);
                    
                    // XOR address (128 bits) with magic cookie + transaction ID
                    // For simplicity, just extract the raw address
                    uint8_t raw_addr[16];
                    memcpy(raw_addr, val + 4, 16);
                    
                    // XOR each 4-byte chunk with cookie
                    for (int i = 0; i < 4; ++i) {
                        uint32_t* p = (uint32_t*)(raw_addr + i * 4);
                        *p = ntohl(*p) ^ kStunMagicCookie;
                    }
                    
                    char addr_str[INET6_ADDRSTRLEN];
                    struct in6_addr in6;
                    memcpy(&in6, raw_addr, 16);
                    inet_ntop(AF_INET6, &in6, addr_str, INET6_ADDRSTRLEN);
                    *address = addr_str;
                    
                    return true;
                }
            }
            
            offset += 4 + attr_len;
            if (attr_len % 4 != 0) {
                offset += 4 - (attr_len % 4);
            }
        }
        
        return false;
    }
    
    StunConfig config_;
    SocketType socket_fd_;
};

// ============================================================================
// Factory Function
// ============================================================================

std::shared_ptr<IIceAgent> CreateIceAgent() {
    return std::make_shared<IceAgent>();
}

}  // namespace minirtc
