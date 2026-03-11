/**
 * @file rtc_error_test.cpp
 * @brief Test file for rtc_error.h and result.h
 */

#include <iostream>
#include <cassert>
#include "minirtc/rtc_error.h"
#include "minirtc/result.h"

using namespace minirtc;

void TestRtcError() {
    std::cout << "Testing RtcError..." << std::endl;
    
    // Test ToString
    assert(std::string(ToString(RtcError::kOk)) == "OK");
    assert(std::string(ToString(RtcError::kInvalidParam)) == "Invalid parameter");
    assert(std::string(ToString(RtcError::kNetworkError)) == "Network error");
    assert(std::string(ToString(RtcError::kCodecError)) == "Codec error");
    
    // Test error category
    std::error_code ec = make_error_code(RtcError::kNotInitialized);
    assert(ec.category() == RtcErrorCategory());
    assert(ec.value() == static_cast<int>(RtcError::kNotInitialized));
    
    // Test std::is_error_code_enum
    static_assert(std::is_error_code_enum<RtcError>::value, "RtcError should be error_code_enum");
    
    std::cout << "  RtcError tests passed!" << std::endl;
}

void TestResultVoid() {
    std::cout << "Testing Result<void>..." << std::endl;
    
    // Test Ok
    auto ok_result = Result<void>::Ok();
    assert(ok_result.IsOk() == true);
    assert(ok_result.ErrorCode() == RtcError::kOk);
    assert(ok_result.ErrorMessage().empty());
    // Can use in if condition
    if (ok_result) {}
    
    // Test Error
    auto err_result = Result<void>::Error(RtcError::kInvalidParam, "Invalid parameter test");
    assert(err_result.IsOk() == false);
    assert(err_result.ErrorCode() == RtcError::kInvalidParam);
    assert(err_result.ErrorMessage() == "Invalid parameter test");
    
    std::cout << "  Result<void> tests passed!" << std::endl;
}

void TestResultInt() {
    std::cout << "Testing Result<int>..." << std::endl;
    
    // Test Ok
    auto ok_result = Result<int>::Ok(42);
    assert(ok_result.IsOk() == true);
    assert(ok_result.Value() == 42);
    assert(ok_result.ValueOr(0) == 42);
    assert(ok_result.ErrorCode() == RtcError::kOk);
    
    // Test Error
    auto err_result = Result<int>::Error(RtcError::kNoMemory, "Out of memory");
    assert(err_result.IsOk() == false);
    assert(err_result.ErrorCode() == RtcError::kNoMemory);
    assert(err_result.ErrorMessage() == "Out of memory");
    assert(err_result.ValueOr(100) == 100);
    
    std::cout << "  Result<int> tests passed!" << std::endl;
}

void TestResultString() {
    std::cout << "Testing Result<std::string>..." << std::endl;
    
    // Test Ok
    auto ok_result = Result<std::string>::Ok("hello");
    assert(ok_result.IsOk() == true);
    assert(ok_result.Value() == "hello");
    
    // Test Error
    auto err_result = Result<std::string>::Error(RtcError::kNetworkError);
    assert(err_result.IsOk() == false);
    assert(err_result.ErrorCode() == RtcError::kNetworkError);
    
    std::cout << "  Result<std::string> tests passed!" << std::endl;
}

void TestResultNullptr() {
    std::cout << "Testing Result<std::nullptr_t>..." << std::endl;
    
    auto ok_result = Result<std::nullptr_t>::Ok();
    assert(ok_result.IsOk() == true);
    assert(ok_result.Value() == nullptr);
    
    auto err_result = Result<std::nullptr_t>::Error(RtcError::kTimeout);
    assert(err_result.IsOk() == false);
    
    std::cout << "  Result<std::nullptr_t> tests passed!" << std::endl;
}

void TestMacros() {
    std::cout << "Testing macros..." << std::endl;
    
    auto ok = MINIRTC_OK;
    assert(ok.IsOk() == true);
    
    auto err = MINIRTC_ERROR(kBusy);
    assert(err.IsOk() == false);
    assert(err.ErrorCode() == RtcError::kBusy);
    
    auto err_msg = MINIRTC_ERROR_MSG(kCodecError, "Codec initialization failed");
    assert(err_msg.ErrorCode() == RtcError::kCodecError);
    assert(err_msg.ErrorMessage() == "Codec initialization failed");
    
    std::cout << "  Macro tests passed!" << std::endl;
}

int main() {
    std::cout << "=== MiniRTC Error System Tests ===" << std::endl;
    
    TestRtcError();
    TestResultVoid();
    TestResultInt();
    TestResultString();
    TestResultNullptr();
    TestMacros();
    
    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}
