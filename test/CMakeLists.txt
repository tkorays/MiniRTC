cmake_minimum_required(VERSION 3.15)

# Test sources
set(COMMON_SOURCES
    common/test_network_emulator.cc
    common/test_nack_module.cc
    common/test_fec_module.cc
)

set(UNIT_SOURCES
    unit/test_unit.cc
)

set(INTEGRATION_SOURCES
    integration/test_local_loop.cc
    integration/test_av_sync.cc
    integration/test_nack_recovery.cc
    integration/test_fec_recovery.cc
)

# Fetch GoogleTest if not already available
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED TRUE)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Build unit tests
add_executable(minirtc_unit_test ${UNIT_SOURCES} ${COMMON_SOURCES})
target_link_libraries(minirtc_unit_test 
    minirtc 
    gtest
    gtest_main
    pthread
)
target_include_directories(minirtc_unit_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)
add_test(NAME minirtc_unit_test COMMAND minirtc_unit_test)

# Build local loop tests
add_executable(minirtc_local_loop_test 
    integration/test_local_loop.cc 
    common/test_network_emulator.cc
    common/test_nack_module.cc
    common/test_fec_module.cc
)
target_link_libraries(minirtc_local_loop_test 
    minirtc 
    gtest
    gtest_main
    pthread
)
target_include_directories(minirtc_local_loop_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)
add_test(NAME minirtc_local_loop_test COMMAND minirtc_local_loop_test)

# Build AV sync tests
add_executable(minirtc_av_sync_test 
    integration/test_av_sync.cc 
    common/test_network_emulator.cc
    common/test_nack_module.cc
)
target_link_libraries(minirtc_av_sync_test 
    minirtc 
    gtest
    gtest_main
    pthread
)
target_include_directories(minirtc_av_sync_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)
add_test(NAME minirtc_av_sync_test COMMAND minirtc_av_sync_test)

# Build NACK recovery tests
add_executable(minirtc_nack_test 
    integration/test_nack_recovery.cc 
    common/test_network_emulator.cc
    common/test_nack_module.cc
)
target_link_libraries(minirtc_nack_test 
    minirtc 
    gtest
    gtest_main
    pthread
)
target_include_directories(minirtc_nack_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)
add_test(NAME minirtc_nack_test COMMAND minirtc_nack_test)

# Build FEC recovery tests
add_executable(minirtc_fec_test 
    integration/test_fec_recovery.cc 
    common/test_network_emulator.cc
    common/test_fec_module.cc
)
target_link_libraries(minirtc_fec_test 
    minirtc 
    gtest
    gtest_main
    pthread
)
target_include_directories(minirtc_fec_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)
add_test(NAME minirtc_fec_test COMMAND minirtc_fec_test)

# Build E2E tests
add_executable(minirtc_e2e_test 
    integration/test_e2e.cc 
    ${PROJECT_SOURCE_DIR}/src/e2e_test.cc
)
target_link_libraries(minirtc_e2e_test 
    minirtc 
    gtest
    gtest_main
    pthread
)
target_include_directories(minirtc_e2e_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)
add_test(NAME minirtc_e2e_test COMMAND minirtc_e2e_test)

# ============================================================================
# New Module Tests
# ============================================================================

# Stream/Track tests
add_executable(minirtc_stream_track_test 
    test_stream_track.cc
)
target_link_libraries(minirtc_stream_track_test 
    minirtc 
    gtest
    gtest_main
    pthread
)
target_include_directories(minirtc_stream_track_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
add_test(NAME minirtc_stream_track_test COMMAND minirtc_stream_track_test)

# JitterBuffer tests
add_executable(minirtc_jitter_buffer_test 
    test_jitter_buffer.cc
)
target_link_libraries(minirtc_jitter_buffer_test 
    minirtc 
    gtest
    gtest_main
    pthread
)
target_include_directories(minirtc_jitter_buffer_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
add_test(NAME minirtc_jitter_buffer_test COMMAND minirtc_jitter_buffer_test)

# H264Packer tests
add_executable(minirtc_h264_packer_test 
    test_h264_packer.cc
)
target_link_libraries(minirtc_h264_packer_test 
    minirtc 
    gtest
    gtest_main
    pthread
)
target_include_directories(minirtc_h264_packer_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
add_test(NAME minirtc_h264_packer_test COMMAND minirtc_h264_packer_test)

# E2E framework tests
add_executable(minirtc_e2e_framework_test 
    integration/test_e2e_framework.cc
    ${PROJECT_SOURCE_DIR}/src/e2e_test.cc
)
target_link_libraries(minirtc_e2e_framework_test 
    minirtc 
    gtest
    gtest_main
    pthread
)
target_include_directories(minirtc_e2e_framework_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
add_test(NAME minirtc_e2e_framework_test COMMAND minirtc_e2e_framework_test)
