# MiniRTC source files
add_library(minirtc_codec OBJECT
    # Transport module
    transport/rtp_packet.cc
    transport/rtcp_packet.cc
    transport/rtp_transport.cc
    transport/rtcp_module.cc
    transport/network_interface.cc
    transport/srtp_transport.cc
    
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
)

target_include_directories(minirtc_codec PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
