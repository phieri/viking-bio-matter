cmake_minimum_required(VERSION 3.15)

project(viking_bio_host_bridge CXX C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Check for Matter SDK
if(NOT DEFINED MATTER_ROOT AND NOT DEFINED CHIP_ROOT)
    if(DEFINED ENV{MATTER_ROOT})
        set(MATTER_ROOT $ENV{MATTER_ROOT})
    elseif(DEFINED ENV{CHIP_ROOT})
        set(MATTER_ROOT $ENV{CHIP_ROOT})
    else()
        message(FATAL_ERROR 
            "MATTER_ROOT or CHIP_ROOT environment variable not set.\n"
            "Please set one of these to point to your connectedhomeip checkout:\n"
            "  export MATTER_ROOT=/path/to/connectedhomeip\n"
            "  export CHIP_ROOT=/path/to/connectedhomeip\n"
            "\n"
            "Recommended: Clone and build Matter SDK first:\n"
            "  git clone https://github.com/project-chip/connectedhomeip.git\n"
            "  cd connectedhomeip\n"
            "  git checkout v1.2.0.1  # or latest stable release\n"
            "  git submodule update --init\n"
            "  source scripts/activate.sh\n"
            "  gn gen out/host\n"
            "  ninja -C out/host\n")
    endif()
endif()

# Verify Matter SDK exists
if(NOT EXISTS "${MATTER_ROOT}/src")
    message(FATAL_ERROR 
        "Matter SDK not found at ${MATTER_ROOT}\n"
        "Please ensure MATTER_ROOT points to a valid connectedhomeip checkout.")
endif()

message(STATUS "Matter SDK found at: ${MATTER_ROOT}")

# Find Matter libraries
# Note: The exact library names and locations may vary by Matter version
set(MATTER_OUT_DIR "${MATTER_ROOT}/out/host" CACHE PATH "Matter build output directory")

if(NOT EXISTS "${MATTER_OUT_DIR}")
    message(WARNING 
        "Matter build output not found at ${MATTER_OUT_DIR}\n"
        "Please build the Matter SDK first:\n"
        "  cd ${MATTER_ROOT}\n"
        "  source scripts/activate.sh\n"
        "  gn gen out/host\n"
        "  ninja -C out/host\n"
        "\n"
        "Continuing with limited build support...")
    set(MATTER_LIBRARIES_AVAILABLE FALSE)
else()
    set(MATTER_LIBRARIES_AVAILABLE TRUE)
    message(STATUS "Matter libraries found at: ${MATTER_OUT_DIR}")
endif()

# Source files
set(HOST_BRIDGE_SOURCES
    main.cpp
    matter_bridge.cpp
    viking_bio_protocol_linux.c
)

# Create executable
add_executable(host_bridge ${HOST_BRIDGE_SOURCES})

# Add Matter SDK includes
target_include_directories(host_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MATTER_ROOT}/src
    ${MATTER_ROOT}/src/include
    ${MATTER_ROOT}/src/lib
    ${MATTER_ROOT}/zzz_generated
    ${MATTER_ROOT}/zzz_generated/app-common
    ${MATTER_ROOT}/third_party/nlassert/repo/include
    ${MATTER_ROOT}/third_party/nlio/repo/include
)

# Enable Matter support
target_compile_definitions(host_bridge PRIVATE
    ENABLE_MATTER=1
    CHIP_HAVE_CONFIG_H=1
)

# Link Matter libraries if available
if(MATTER_LIBRARIES_AVAILABLE)
    # Find all Matter libraries
    # Note: The exact library names may vary by Matter version
    # Common libraries needed for a basic bridge:
    set(MATTER_LIBS
        chip
        ChipDataModel
        ChipCrypto
        ChipProtocols
        ChipController
        ChipDeviceController
        Inet
        ChipShell
        SetupPayload
    )
    
    # Try to find and link each library
    foreach(lib ${MATTER_LIBS})
        set(lib_path "${MATTER_OUT_DIR}/lib/lib${lib}.a")
        if(EXISTS "${lib_path}")
            target_link_libraries(host_bridge ${lib_path})
            message(STATUS "Linking Matter library: ${lib}")
        else()
            # Try alternative naming
            set(lib_path "${MATTER_OUT_DIR}/obj/src/lib${lib}.a")
            if(EXISTS "${lib_path}")
                target_link_libraries(host_bridge ${lib_path})
                message(STATUS "Linking Matter library: ${lib}")
            else()
                message(WARNING "Matter library not found: ${lib} (this may be OK if not needed)")
            endif()
        endif()
    endforeach()
    
    # Link system libraries required by Matter
    target_link_libraries(host_bridge
        pthread
        dl
        rt
    )
else()
    message(WARNING 
        "Building stub version without Matter libraries.\n"
        "The executable will compile but Matter functionality will be disabled.\n"
        "To enable full Matter support, build the Matter SDK first.")
endif()

# Compiler flags
target_compile_options(host_bridge PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# Install target
install(TARGETS host_bridge
    RUNTIME DESTINATION bin
)

# Print build information
message(STATUS "Host bridge configuration:")
message(STATUS "  Matter SDK: ${MATTER_ROOT}")
message(STATUS "  Matter libraries: ${MATTER_LIBRARIES_AVAILABLE}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
