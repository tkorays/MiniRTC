# MiniRTC source files
add_library(minirtc_codec OBJECT
    # Transport module
    transport/rtp_packet.cc
    transport/rtcp_packet.cc
    transport/rtp_transport.cc
    transport/rtcp_module.cc
    transport/network_interface.cc
    transport/srtp_transport.cc
    
    # DTLS/SRTP module
    dtls_srtp.cc
    
    # Capture/Render module (new implementation)
    capture_render_factory.cc
    fake_video_capture.cc
    fake_audio_capture.cc
    fake_video_renderer.cc
    fake_audio_player.cc
    
    # NACK/FEC module
    packet_cache.cc
    nack_module.cc
    fec_module.cc

    # Jitter Buffer module
    jitter_buffer.cc
    
    # Stream/Track module
    stream_track.cc
    
    # Codec module
    codec/h264_packer.cc
    
    # E2E test module
    e2e_test.cc
    
    # Bandwidth Estimator module
    bandwidth_estimator.cc
    
    # ICE module
    ice.cc
    
    # PeerConnection module
    peer_connection.cc
    
    # Memory Pool module
    memory_pool.cc

    # Media Synchronizer module
    media_synchronizer.cc
)

target_include_directories(minirtc_codec PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# Opus support - check for both opus::opus and plain opus targets
if(TARGET opus::opus)
    message(STATUS "Opus found (opus::opus), enabling MINIRTC_USE_OPUS")
    target_compile_definitions(minirtc_codec PUBLIC MINIRTC_USE_OPUS=1)
    target_link_libraries(minirtc_codec PUBLIC opus::opus)
elseif(TARGET opus)
    message(STATUS "Opus found (opus), enabling MINIRTC_USE_OPUS")
    target_compile_definitions(minirtc_codec PUBLIC MINIRTC_USE_OPUS=1)
    target_link_libraries(minirtc_codec PUBLIC opus)
else()
    message(STATUS "Opus not found, using stub implementation")
endif()

# OpenH264 support - use pre-built library if available
# Set OPENH264_LIBRARY and OPENH264_INCLUDE_DIR in parent CMakeLists.txt
if(MINIRTC_USE_H264 AND EXISTS ${OPENH264_LIBRARY})
    message(STATUS "OpenH264 library found: ${OPENH264_LIBRARY}")
    target_compile_definitions(minirtc_codec PUBLIC MINIRTC_USE_H264=1)
    target_include_directories(minirtc_codec PUBLIC ${OPENH264_INCLUDE_DIR})
    target_link_libraries(minirtc_codec PUBLIC ${OPENH264_LIBRARY})
elseif(MINIRTC_USE_H264)
    message(WARNING "OpenH264 library not found at ${OPENH264_LIBRARY}, using stub implementation")
endif()

# FFmpeg support
if(MINIRTC_USE_FFMPEG)
    message(STATUS "FFmpeg found, enabling MINIRTC_USE_FFMPEG")
    target_compile_definitions(minirtc_codec PUBLIC MINIRTC_USE_FFMPEG=1)
    target_link_libraries(minirtc_codec PUBLIC ${FFMPEG_LIBRARIES})
    target_include_directories(minirtc_codec PUBLIC ${FFMPEG_INCLUDE_DIRS})
endif()
