cmake_minimum_required(VERSION 3.15)
project(MiniRTC VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" OFF)
option(USE_OPENH264 "Use openh264 for H.264 encoding" OFF)
option(USE_FFMPEG "Use FFmpeg for H.264 encoding/decoding" OFF)

# H.264 codec selection
# Note: When OpenH264 source is available, set USE_OPENH264=ON and provide OPENH264_LIBRARY path
if(USE_OPENH264 AND NOT USE_FFMPEG)
    set(MINIRTC_USE_H264 ON)
    message(STATUS "Using OpenH264 for H.264 encoding/decoding")
elseif(USE_FFMPEG)
    set(MINIRTC_USE_FFMPEG ON)
    message(STATUS "Using FFmpeg for H.264 encoding/decoding")
else()
    message(STATUS "Using stub H.264 implementation (set USE_OPENH264=ON to enable OpenH264)")
endif()

# OpenH264 library path (set this to your pre-built OpenH264 library path)
# Example: set(OPENH264_LIBRARY "/path/to/libopenh264.a")
# Example: set(OPENH264_INCLUDE_DIR "/path/to/openh264/codec/api/wels")
if(MINIRTC_USE_H264 AND EXISTS ${OPENH264_LIBRARY})
    message(STATUS "OpenH264 library found: ${OPENH264_LIBRARY}")
elseif(MINIRTC_USE_H264)
    message(WARNING "OpenH264 library not found at ${OPENH264_LIBRARY}, using stub implementation")
    set(MINIRTC_USE_H264 OFF)
endif()

# Fetch FFmpeg libraries if enabled
if(MINIRTC_USE_FFMPEG)
    # FFmpeg would be found via pkg-config or system installation
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libswscale libavutil)
    message(STATUS "FFmpeg found: ${FFMPEG_VERSION}")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# ===== Find Opus (use brew if available) =====
# Skip FetchContent if we have system opus
find_path(OPUS_INCLUDE_DIR opus.h /usr/local/include /opt/homebrew/include /usr/include)
find_library(OPUS_LIBRARY NAMES opus PATHS /usr/local/lib /opt/homebrew/lib /usr/lib)

if(OPUS_INCLUDE_DIR AND OPUS_LIBRARY)
    message(STATUS "Found Opus: ${OPUS_LIBRARY}")
    set(OPUS_FOUND TRUE)
else()
    # Fall back to FetchContent
    include(FetchContent)
    FetchContent_Declare(
        opus
        GIT_REPOSITORY https://github.com/xiph/opus.git
        GIT_TAG v1.5.2
        GIT_SHALLOW TRUE
    )
    message(STATUS "Fetching Opus...")
    FetchContent_MakeAvailable(opus)
endif()

# Add subdirectories
add_subdirectory(src)

# Build static library
add_library(minirtc STATIC $<TARGET_OBJECTS:minirtc_codec>)

target_include_directories(minirtc PUBLIC 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(minirtc PROPERTIES 
    OUTPUT_NAME "minirtc"
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Install targets
install(TARGETS minirtc
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ 
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Build tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Build examples
if(BUILD_EXAMPLES)
    message(STATUS "Building examples...")
    add_subdirectory(examples)
endif()

# Print configuration
message(STATUS "MiniRTC Version: ${PROJECT_VERSION}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Build examples: ${BUILD_EXAMPLES}")
