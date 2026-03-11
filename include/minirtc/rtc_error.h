/**
 * @file rtc_error.h
 * @brief MiniRTC unified error codes
 */

#ifndef MINIRTC_RTC_ERROR_H_
#define MINIRTC_RTC_ERROR_H_

#include <string>
#include <system_error>

namespace minirtc {

// =============================================================================
// Unified Error Codes
// =============================================================================

// 统一错误码
enum class RtcError {
    // Success
    kOk = 0,
    
    // 通用错误 (-1 ~ -99)
    kInvalidParam = -1,
    kNotInitialized = -2,
    kAlreadyInitialized = -3,
    kNotSupported = -4,
    kNoMemory = -5,
    kTimeout = -6,
    kBusy = -7,
    kClosed = -8,
    kInternalError = -9,
    kUnexpected = -10,
    
    // 网络错误 (-100 ~ -199)
    kNetworkError = -100,
    kConnectionFailed = -101,
    kConnectionRefused = -102,
    kConnectionReset = -103,
    kHostUnreachable = -104,
    kPortInUse = -105,
    kSocketError = -106,
    kBufferOverflow = -107,
    kBufferUnderflow = -108,
    
    // 媒体错误 (-200 ~ -299)
    kNoVideo = -200,
    kNoAudio = -201,
    kCodecError = -202,
    kCaptureError = -203,
    kRenderError = -204,
    kEncoderError = -205,
    kDecoderError = -206,
    kInvalidStream = -207,
    kFrameTooLarge = -208,
    
    // 安全错误 (-300 ~ -399)
    kCryptoError = -300,
    kAuthenticationFailed = -301,
    kVerificationFailed = -302,
    kDtlsError = -303,
    kSrtpError = -304,
    kIceError = -305,
    
    // 设备错误 (-400 ~ -499)
    kDeviceNotFound = -400,
    kDeviceInUse = -401,
    kDeviceError = -402,
    kPermissionDenied = -403,
};

// =============================================================================
// Error to String Conversion
// =============================================================================

inline const char* ToString(RtcError error) {
    switch (error) {
        // Success
        case RtcError::kOk: return "OK";
        
        // Generic errors
        case RtcError::kInvalidParam: return "Invalid parameter";
        case RtcError::kNotInitialized: return "Not initialized";
        case RtcError::kAlreadyInitialized: return "Already initialized";
        case RtcError::kNotSupported: return "Not supported";
        case RtcError::kNoMemory: return "Out of memory";
        case RtcError::kTimeout: return "Operation timeout";
        case RtcError::kBusy: return "Resource busy";
        case RtcError::kClosed: return "Resource closed";
        case RtcError::kInternalError: return "Internal error";
        case RtcError::kUnexpected: return "Unexpected error";
        
        // Network errors
        case RtcError::kNetworkError: return "Network error";
        case RtcError::kConnectionFailed: return "Connection failed";
        case RtcError::kConnectionRefused: return "Connection refused";
        case RtcError::kConnectionReset: return "Connection reset";
        case RtcError::kHostUnreachable: return "Host unreachable";
        case RtcError::kPortInUse: return "Port already in use";
        case RtcError::kSocketError: return "Socket error";
        case RtcError::kBufferOverflow: return "Buffer overflow";
        case RtcError::kBufferUnderflow: return "Buffer underflow";
        
        // Media errors
        case RtcError::kNoVideo: return "No video";
        case RtcError::kNoAudio: return "No audio";
        case RtcError::kCodecError: return "Codec error";
        case RtcError::kCaptureError: return "Capture error";
        case RtcError::kRenderError: return "Render error";
        case RtcError::kEncoderError: return "Encoder error";
        case RtcError::kDecoderError: return "Decoder error";
        case RtcError::kInvalidStream: return "Invalid stream";
        case RtcError::kFrameTooLarge: return "Frame too large";
        
        // Security errors
        case RtcError::kCryptoError: return "Crypto error";
        case RtcError::kAuthenticationFailed: return "Authentication failed";
        case RtcError::kVerificationFailed: return "Verification failed";
        case RtcError::kDtlsError: return "DTLS error";
        case RtcError::kSrtpError: return "SRTP error";
        case RtcError::kIceError: return "ICE error";
        
        // Device errors
        case RtcError::kDeviceNotFound: return "Device not found";
        case RtcError::kDeviceInUse: return "Device in use";
        case RtcError::kDeviceError: return "Device error";
        case RtcError::kPermissionDenied: return "Permission denied";
        
        default: return "Unknown error";
    }
}

// =============================================================================
// Error Category for std::error_code
// =============================================================================

class RtcErrorCategoryImpl : public std::error_category {
public:
    const char* name() const noexcept override { return "minirtc"; }
    
    std::string message(int ev) const override { 
        return ToString(static_cast<RtcError>(ev)); 
    }
    
    bool equivalent(const std::error_code& code, int condition) const noexcept override {
        return *this == code.category() && 
               static_cast<int>(static_cast<RtcError>(code.value())) == condition;
    }
};

inline std::error_category& RtcErrorCategory() {
    static RtcErrorCategoryImpl instance;
    return instance;
}

// Make RtcError work with std::error_code
inline std::error_code make_error_code(RtcError e) {
    return std::error_code(static_cast<int>(e), RtcErrorCategory());
}

// =============================================================================
// Legacy Type Aliases (for backward compatibility)
// =============================================================================

// Note: The following type aliases map old error codes to new unified codes.
// These are provided for gradual migration and should be removed in future versions.

// Map minirtc_status_t to RtcError
inline RtcError ToRtcError(int status) {
    switch (status) {
        case 0: return RtcError::kOk;
        case -1: return RtcError::kInternalError;
        case -2: return RtcError::kInvalidParam;
        case -3: return RtcError::kInvalidParam;
        case -4: return RtcError::kNoMemory;
        case -5: return RtcError::kTimeout;
        case -6: return RtcError::kNotInitialized;
        case -7: return RtcError::kAlreadyInitialized;
        default: return RtcError::kUnexpected;
    }
}

}  // namespace minirtc

// Enable std::is_error_code_enum for RtcError
namespace std {
    template<>
    struct is_error_code_enum<minirtc::RtcError> : true_type {};
}

#endif  // MINIRTC_RTC_ERROR_H_
