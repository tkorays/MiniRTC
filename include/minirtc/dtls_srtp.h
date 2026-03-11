/**
 * @file dtls_srtp.h
 * @brief MiniRTC DTLS/SRTP transport interface
 */

#ifndef MINIRTC_DTLS_SRTP_H_
#define MINIRTC_DTLS_SRTP_H_

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "minirtc/rtc_error.h"
#include "minirtc/result.h"

namespace minirtc {

// =============================================================================
// SRTP Crypto Suite
// =============================================================================

// SRTP加密算法
enum class SrtpCryptoSuite {
    kAes128CmSha1_80,   // AES-CM-HMAC-SHA1-80
    kAes128CmSha1_32,   // AES-CM-HMAC-SHA1-32
    kAes256GcmaSha1_80, // AES-GCM-HMAC-SHA1-80
    kAes256GcmaSha1_128 // AES-GCM-HMAC-SHA1-128
};

// =============================================================================
// DTLS Configuration
// =============================================================================

// DTLS配置
struct DtlsConfig {
    std::string certificate;      // 证书 (PEM)
    std::string private_key;     // 私钥 (PEM)
    bool verify_peer = true;     // 验证对端证书
    int timeout_ms = 5000;       // 超时
    std::string srtp_profiles;   // SRTP profile列表 (如 "SRTP_AES128_CM_SHA1_80")
};

// =============================================================================
// SRTP Configuration
// =============================================================================

// SRTP配置
struct SrtpConfig {
    SrtpCryptoSuite send_suite = SrtpCryptoSuite::kAes128CmSha1_80;
    SrtpCryptoSuite recv_suite = SrtpCryptoSuite::kAes128CmSha1_80;
    std::vector<uint8_t> send_key;    // 发送密钥
    std::vector<uint8_t> recv_key;    // 接收密钥
    std::vector<uint8_t> send_salt;   // 发送盐值
    std::vector<uint8_t> recv_salt;   // 接收盐值
};

// =============================================================================
// DTLS State
// =============================================================================

// DTLS状态
enum class DtlsState {
    kNew,         // 新建，未开始
    kConnecting,  // 连接中
    kConnected,   // 已连接
    kFailed,      // 失败
    kClosed       // 已关闭
};

// =============================================================================
// DTLS Handler Interface
// =============================================================================

// IDtlsHandler接口 - 接收DTLS事件回调
class IDtlsHandler {
public:
    virtual ~IDtlsHandler() = default;
    
    /**
     * @brief DTLS状态变化回调
     * @param state 新的DTLS状态
     */
    virtual void OnDtlsStateChange(DtlsState state) = 0;
    
    /**
     * @brief SRTP密钥就绪回调
     * @param config SRTP配置，包含加密密钥
     */
    virtual void OnSrtpKeyReady(const SrtpConfig& config) = 0;
    
    /**
     * @brief 错误回调
     * @param msg 错误消息
     */
    virtual void OnError(const std::string& msg) = 0;
};

// =============================================================================
// DTLS Transport Interface
// =============================================================================

// IDtlsTransport接口 - DTLS传输层
class IDtlsTransport {
public:
    using Ptr = std::shared_ptr<IDtlsTransport>;
    virtual ~IDtlsTransport() = default;
    
    /**
     * @brief 初始化DTLS传输层
     * @param config DTLS配置
     * @return 初始化结果
     */
    virtual Result<void> Initialize(const DtlsConfig& config) = 0;
    
    /**
     * @brief 设置事件回调处理器
     * @param handler 回调处理器
     */
    virtual void SetHandler(std::shared_ptr<IDtlsHandler> handler) = 0;
    
    /**
     * @brief 开始DTLS握手（作为客户端）
     * @return 握手结果
     */
    virtual Result<void> StartHandshake() = 0;
    
    /**
     * @brief 开始DTLS握手（作为服务端）
     * @return 握手结果
     */
    virtual Result<void> StartAccept() = 0;
    
    /**
     * @brief 传入从网络接收的数据
     * @param data 数据指针
     * @param len 数据长度
     */
    virtual void OnDataReceived(const uint8_t* data, size_t len) = 0;
    
    /**
     * @brief 发送数据到网络
     * @param data 数据指针
     * @param len 数据长度
     * @return 发送的字节数或错误
     */
    virtual Result<size_t> SendData(const uint8_t* data, size_t len) = 0;
    
    /**
     * @brief 获取SRTP保护后的RTP数据
     * @param buffer 输出缓冲区
     * @param rtp_data 原始RTP数据
     * @param len 数据长度
     * @return 加密后的数据长度或错误
     */
    virtual Result<size_t> ProtectRtp(uint8_t* buffer, const uint8_t* rtp_data, size_t len) = 0;
    
    /**
     * @brief 解密SRTP数据
     * @param buffer 输出缓冲区
     * @param srtp_data SRTP加密数据
     * @param len 数据长度
     * @return 解密后的数据长度或错误
     */
    virtual Result<size_t> UnprotectRtp(uint8_t* buffer, const uint8_t* srtp_data, size_t len) = 0;
    
    /**
     * @brief 获取当前DTLS状态
     * @return DTLS状态
     */
    virtual DtlsState GetState() const = 0;
    
    /**
     * @brief 获取本地SRTP配置
     * @return SRTP配置
     */
    virtual SrtpConfig GetSrtpConfig() const = 0;
    
    /**
     * @brief 关闭DTLS连接
     * @return 关闭结果
     */
    virtual Result<void> Close() = 0;
};

// =============================================================================
// Factory Function
// =============================================================================

/**
 * @brief 创建DTLS Transport实例
 * @return DTLS Transport智能指针
 * @note 根据编译条件返回OpenSSL实现或stub实现
 */
std::shared_ptr<IDtlsTransport> CreateDtlsTransport();

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief 将SrtpCryptoSuite转换为字符串
 * @param suite 加密套件
 * @return 字符串表示
 */
const char* SrtpCryptoSuiteToString(SrtpCryptoSuite suite);

/**
 * @brief 获取默认的SRTP profile字符串
 * @return 默认profile
 */
const char* GetDefaultSrtpProfile();

}  // namespace minirtc

#endif  // MINIRTC_DTLS_SRTP_H_
