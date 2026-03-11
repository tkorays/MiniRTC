/**
 * @file result.h
 * @brief MiniRTC Result wrapper for error handling
 */

#ifndef MINIRTC_RESULT_H_
#define MINIRTC_RESULT_H_

#include <type_traits>
#include <utility>
#include <string>
#include "rtc_error.h"

namespace minirtc {

// =============================================================================
// Result<T> - Generic Result Type
// =============================================================================

template<typename T>
class Result {
public:
    /**
     * @brief Create a success result with value
     * @param value The success value
     * @return Result<T>
     */
    static Result<T> Ok(T value) {
        Result<T> result;
        result.value_ = std::move(value);
        result.error_ = RtcError::kOk;
        return result;
    }
    
    /**
     * @brief Create an error result
     * @param code Error code
     * @param msg Optional error message
     * @return Result<T>
     */
    static Result<T> Error(RtcError code, const std::string& msg = "") {
        Result<T> result;
        result.error_ = code;
        result.message_ = msg;
        return result;
    }
    
    /**
     * @brief Check if result is success
     * @return true if success
     */
    bool IsOk() const { return error_ == RtcError::kOk; }
    
    /**
     * @brief Check if result is error
     * @return true if error
     */
    bool IsError() const { return !IsOk(); }
    
    /**
     * @brief Get the value (only valid if IsOk())
     * @return Reference to the value
     * @note Undefined behavior if IsOk() is false
     */
    T& Value() { return value_; }
    
    /**
     * @brief Get the value (only valid if IsOk())
     * @return Const reference to the value
     * @note Undefined behavior if IsOk() is false
     */
    const T& Value() const { return value_; }
    
    /**
     * @brief Get the value or default
     * @param default_value Value to return if error
     * @return Value or default
     */
    T ValueOr(T default_value) const {
        return IsOk() ? value_ : default_value;
    }
    
    /**
     * @brief Get error code
     * @return Error code
     */
    RtcError ErrorCode() const { return error_; }
    
    /**
     * @brief Get error message
     * @return Error message string
     */
    const std::string& ErrorMessage() const { return message_; }
    
    /**
     * @brief Bool conversion operator
     * @return true if success
     */
    explicit operator bool() const { return IsOk(); }
    
    // Copy and move
    Result(const Result& other) = default;
    Result& operator=(const Result& other) = default;
    Result(Result&& other) noexcept = default;
    Result& operator=(Result&& other) noexcept = default;
    
private:
    Result() = default;
    
    T value_;
    RtcError error_ = RtcError::kOk;
    std::string message_;
};

// =============================================================================
// Result<void> - Specialization for void
// =============================================================================

template<>
class Result<void> {
public:
    /**
     * @brief Create a success result
     * @return Result<void>
     */
    static Result<void> Ok() {
        Result<void> result;
        result.error_ = RtcError::kOk;
        return result;
    }
    
    /**
     * @brief Create an error result
     * @param code Error code
     * @param msg Optional error message
     * @return Result<void>
     */
    static Result<void> Error(RtcError code, const std::string& msg = "") {
        Result<void> result;
        result.error_ = code;
        result.message_ = msg;
        return result;
    }
    
    /**
     * @brief Check if result is success
     * @return true if success
     */
    bool IsOk() const { return error_ == RtcError::kOk; }
    
    /**
     * @brief Check if result is error
     * @return true if error
     */
    bool IsError() const { return !IsOk(); }
    
    /**
     * @brief Get error code
     * @return Error code
     */
    RtcError ErrorCode() const { return error_; }
    
    /**
     * @brief Get error message
     * @return Error message string
     */
    const std::string& ErrorMessage() const { return message_; }
    
    /**
     * @brief Bool conversion operator
     * @return true if success
     */
    explicit operator bool() const { return IsOk(); }
    
    Result(const Result& other) = default;
    Result& operator=(const Result& other) = default;
    Result(Result&& other) noexcept = default;
    Result& operator=(Result&& other) noexcept = default;
    
private:
    Result() = default;
    
    RtcError error_ = RtcError::kOk;
    std::string message_;
};

// =============================================================================
// Result<std::nullptr_t> - For operations that return nothing on success
// =============================================================================

template<>
class Result<std::nullptr_t> {
public:
    /**
     * @brief Create a success result
     * @return Result<std::nullptr_t>
     */
    static Result<std::nullptr_t> Ok() {
        Result<std::nullptr_t> result;
        result.error_ = RtcError::kOk;
        return result;
    }
    
    /**
     * @brief Create an error result
     * @param code Error code
     * @param msg Optional error message
     * @return Result<std::nullptr_t>
     */
    static Result<std::nullptr_t> Error(RtcError code, const std::string& msg = "") {
        Result<std::nullptr_t> result;
        result.error_ = code;
        result.message_ = msg;
        return result;
    }
    
    /**
     * @brief Check if result is success
     * @return true if success
     */
    bool IsOk() const { return error_ == RtcError::kOk; }
    
    /**
     * @brief Check if result is error
     * @return true if error
     */
    bool IsError() const { return !IsOk(); }
    
    /**
     * @brief Get error code
     * @return Error code
     */
    RtcError ErrorCode() const { return error_; }
    
    /**
     * @brief Get error message
     * @return Error message string
     */
    const std::string& ErrorMessage() const { return message_; }
    
    /**
     * @brief Bool conversion operator
     * @return true if success
     */
    explicit operator bool() const { return IsOk(); }
    
    /**
     * @brief Get nullptr value
     * @return nullptr
     */
    std::nullptr_t Value() const { return nullptr; }
    
    Result(const Result& other) = default;
    Result& operator=(const Result& other) = default;
    Result(Result&& other) noexcept = default;
    Result& operator=(Result&& other) noexcept = default;
    
private:
    Result() = default;
    
    RtcError error_ = RtcError::kOk;
    std::string message_;
};

// =============================================================================
// Utility Macros (optional convenience)
// =============================================================================

// Macro to create error result easily
#define MINIRTC_OK ::minirtc::Result<void>::Ok()

#define MINIRTC_ERROR(code) ::minirtc::Result<void>::Error(::minirtc::RtcError::code)
#define MINIRTC_ERROR_MSG(code, msg) ::minirtc::Result<void>::Error(::minirtc::RtcError::code, msg)

#define MINIRTC_RETURN_ERROR(code) return ::minirtc::Result<void>::Error(::minirtc::RtcError::code)
#define MINIRTC_RETURN_ERROR_MSG(code, msg) return ::minirtc::Result<void>::Error(::minirtc::RtcError::code, msg)

}  // namespace minirtc

#endif  // MINIRTC_RESULT_H_
