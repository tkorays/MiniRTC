# Examples CMakeLists.txt

# Create minirtc_demo executable
add_executable(minirtc_demo 
    minirtc_demo.cc
)

# Link with minirtc library
target_link_libraries(minirtc_demo 
    minirtc
)

# Include directories
target_include_directories(minirtc_demo PRIVATE
    ${PROJECT_SOURCE_DIR}/include
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
