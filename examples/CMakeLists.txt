# Examples CMakeLists.txt

# Create minirtc_demo executable
add_executable(minirtc_demo 
    minirtc_demo.cc
)

# Include directories
target_include_directories(minirtc_demo PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Link with minirtc library
target_link_libraries(minirtc_demo 
    minirtc
)

# Add FFmpeg libraries if available
if(USE_FFMPEG)
    # Link with FFmpeg using explicit paths
    target_link_libraries(minirtc_demo 
        /usr/local/lib/libavformat.dylib
        /usr/local/lib/libavcodec.dylib
        /usr/local/lib/libswscale.dylib
        /usr/local/lib/libswresample.dylib
        /usr/local/lib/libavutil.dylib
    )
    message(STATUS "Linking with FFmpeg dylibs")
else()
    # Fall back to OpenH264 if not using FFmpeg
    target_link_libraries(minirtc_demo 
        /usr/local/opt/openh264/lib/libopenh264.dylib
    )
    message(STATUS "Linking with OpenH264 library")
endif()

# Add Opus (always needed)
target_link_libraries(minirtc_demo 
    /usr/local/opt/opus/lib/libopus.a
)

# Platform-specific settings
if(APPLE)
    target_compile_definitions(minirtc_demo PRIVATE 
        __APPLE__
    )
elseif(UNIX)
    target_compile_definitions(minirtc_demo PRIVATE 
        __LINUX__
    )
elseif(WIN32)
    target_compile_definitions(minirtc_demo PRIVATE 
        _WIN32
    )
endif()

# Installation
install(TARGETS minirtc_demo
    RUNTIME DESTINATION bin
)
