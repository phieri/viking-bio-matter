cmake_minimum_required(VERSION 3.16)

# Host bridge CMake configuration
project(viking_bio_host_bridge CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Initially assume Matter will be enabled
set(MATTER_ENABLED ${ENABLE_MATTER})

# If Matter is enabled, check for MATTER_ROOT or CHIP_ROOT
if(ENABLE_MATTER)
    if(DEFINED ENV{MATTER_ROOT})
        set(MATTER_ROOT $ENV{MATTER_ROOT})
    elseif(DEFINED ENV{CHIP_ROOT})
        set(MATTER_ROOT $ENV{CHIP_ROOT})
    else()
        message(WARNING 
            "ENABLE_MATTER is ON but MATTER_ROOT (or CHIP_ROOT) environment variable is not set.\n"
            "Building in stub mode without Matter SDK.\n"
            "To enable full Matter support, set MATTER_ROOT to point to your connectedhomeip SDK checkout.\n"
            "Example: export MATTER_ROOT=/path/to/connectedhomeip\n"
            "See host_bridge/README.md for instructions on obtaining the Matter SDK.")
        set(MATTER_ENABLED OFF)
    endif()
    
    if(MATTER_ENABLED AND NOT EXISTS "${MATTER_ROOT}")
        message(WARNING 
            "MATTER_ROOT is set to '${MATTER_ROOT}' but this directory does not exist.\n"
            "Building in stub mode without Matter SDK.\n"
            "Please ensure MATTER_ROOT points to a valid connectedhomeip SDK checkout.")
        set(MATTER_ENABLED OFF)
    endif()
    
    if(MATTER_ENABLED)
        message(STATUS "Host bridge: Using Matter SDK at: ${MATTER_ROOT}")
        
        # Add Matter SDK include directories
        set(MATTER_INCLUDE_DIRS
            ${MATTER_ROOT}/src
            ${MATTER_ROOT}/src/include
            ${MATTER_ROOT}/src/lib
            ${MATTER_ROOT}/zzz_generated/app-common
            ${MATTER_ROOT}/examples/platform/linux
            ${MATTER_ROOT}/third_party/nlio/repo/include
            ${MATTER_ROOT}/third_party/nlassert/repo/include
        )
        
        # Add Matter SDK library directories
        set(MATTER_LIB_DIRS
            ${MATTER_ROOT}/out/host
            ${MATTER_ROOT}/out/linux-x64-chip-tool/lib
            ${MATTER_ROOT}/out/host/lib
        )
        
        # Look for Matter libraries (names may vary by version)
        find_library(LIBCHIP chip PATHS ${MATTER_LIB_DIRS} NO_DEFAULT_PATH)
        
        if(NOT LIBCHIP)
            message(WARNING 
                "Could not find Matter libraries in ${MATTER_LIB_DIRS}.\n"
                "Make sure you have built the Matter SDK.\n"
                "Typical build command: cd ${MATTER_ROOT} && ./scripts/build/build_examples.py --target linux-x64-chip-tool build\n"
                "Building in stub mode without Matter SDK.")
            set(MATTER_ENABLED OFF)
        else()
            message(STATUS "Host bridge: Found Matter libraries")
        endif()
    endif()
else()
    message(STATUS "Host bridge: ENABLE_MATTER is OFF, building stub version")
endif()

# Create executable
add_executable(host_bridge
    main.cpp
    matter_bridge.cpp
    viking_bio_protocol_linux.c
)

# Add compile definitions
if(MATTER_ENABLED)
    target_compile_definitions(host_bridge PRIVATE ENABLE_MATTER)
    target_include_directories(host_bridge PRIVATE ${MATTER_INCLUDE_DIRS})
    target_link_directories(host_bridge PRIVATE ${MATTER_LIB_DIRS})
    
    # Link Matter libraries
    # Note: Library names and dependencies may vary by Matter SDK version
    target_link_libraries(host_bridge PRIVATE
        chip
        pthread
        dl
    )
    message(STATUS "Host bridge: Building with full Matter SDK support")
else()
    message(STATUS "Host bridge: Building in stub mode (no Matter SDK)")
endif()

# Always link standard libraries
target_link_libraries(host_bridge PRIVATE pthread)

# Install target
install(TARGETS host_bridge DESTINATION bin)

message(STATUS "Host bridge configuration complete")
