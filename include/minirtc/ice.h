#ifndef MINIRTC_ICE_H_
#define MINIRTC_ICE_H_

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace minirtc {

// ICE candidate类型
enum class IceCandidateType {
    kHost,     // 本地候选
    kSrflx,    // 服务器 reflexive (STUN)
    kPrflx,    // 对等 reflexive (TURN)
    kRelayed   // 中继候选 (TURN)
};

// ICE协议
enum class IceProtocol {
    kUdp,
    kTcp
};

// ICE候选
struct IceCandidate {
    uint32_t foundation = 0;
    uint32_t component_id = 1;
    IceProtocol protocol = IceProtocol::kUdp;
    uint32_t priority = 0;
    std::string host_addr;     // 主机地址
    std::string base_addr;     // 基础地址
    uint16_t port = 0;
    IceCandidateType type = IceCandidateType::kHost;
    std::string transport_addr; // 传输地址
};

// STUN消息类型
enum class StunMessageType {
    kBindingRequest = 0x0001,
    kBindingSuccessResponse = 0x0101,
    kBindingErrorResponse = 0x0111,
};

// STUN属性类型
enum class StunAttributeType {
    kMappedAddress = 0x0001,
    kXorMappedAddress = 0x0020,
    kSoftware = 0x8022,
    kAlternateServer = 0x8023,
    kFingerprint = 0x8028,
};

// STUN配置
struct StunConfig {
    std::vector<std::string> servers = {
        "stun.l.google.com:19302",
        "stun1.l.google.com:19302"
    };
    int timeout_ms = 1000;
    int max_retries = 3;
};

// IIceAgent接口
class IIceAgent {
public:
    using Ptr = std::shared_ptr<IIceAgent>;
    virtual ~IIceAgent() = default;
    
    // 初始化
    virtual bool Initialize(const StunConfig& config) = 0;
    
    // 收集本地候选
    virtual std::vector<IceCandidate> GatherCandidates() = 0;
    
    // 执行STUN绑定请求
    virtual bool Bind(const IceCandidate& candidate, 
                      IceCandidate* mapped_candidate) = 0;
    
    // 获取本地IP
    static std::vector<std::string> GetLocalIPs();
};

// 创建ICE Agent
std::shared_ptr<IIceAgent> CreateIceAgent();

}  // namespace minirtc

#endif
