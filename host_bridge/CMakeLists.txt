cmake_minimum_required(VERSION 3.13)

project(viking_bio_host_bridge CXX C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Check if MATTER_ROOT is set when ENABLE_MATTER is ON
if(ENABLE_MATTER)
    if(NOT DEFINED ENV{MATTER_ROOT} AND NOT DEFINED ENV{CHIP_ROOT})
        message(FATAL_ERROR 
            "ENABLE_MATTER is ON but MATTER_ROOT or CHIP_ROOT environment variable is not set.\n"
            "Please set MATTER_ROOT to point to your connectedhomeip installation:\n"
            "  export MATTER_ROOT=/path/to/connectedhomeip\n"
            "Or set CHIP_ROOT:\n"
            "  export CHIP_ROOT=/path/to/connectedhomeip\n"
            "\n"
            "See host_bridge/README.md for detailed instructions on obtaining and building\n"
            "the Matter SDK.")
    endif()
    
    # Set MATTER_ROOT from either MATTER_ROOT or CHIP_ROOT
    if(DEFINED ENV{MATTER_ROOT})
        set(MATTER_ROOT $ENV{MATTER_ROOT})
    else()
        set(MATTER_ROOT $ENV{CHIP_ROOT})
    endif()
    
    message(STATUS "Matter SDK path: ${MATTER_ROOT}")
    
    # Add Matter SDK includes
    include_directories(
        ${MATTER_ROOT}/src
        ${MATTER_ROOT}/src/include
        ${MATTER_ROOT}/third_party/nlassert/repo/include
        ${MATTER_ROOT}/third_party/nlio/repo/include
        ${MATTER_ROOT}/zzz_generated/app-common
    )
    
    # Define ENABLE_MATTER for conditional compilation
    add_definitions(-DENABLE_MATTER)
    
    # Matter library paths (adjust based on your Matter build configuration)
    set(MATTER_LIB_DIR ${MATTER_ROOT}/out/host)
    
    # Try to find Matter libraries
    find_library(CHIP_LIB chip PATHS ${MATTER_LIB_DIR} NO_DEFAULT_PATH)
    find_library(CHIP_APP_LIB ChipDataModel PATHS ${MATTER_LIB_DIR} NO_DEFAULT_PATH)
    
    if(NOT CHIP_LIB)
        message(WARNING 
            "Could not find Matter libraries in ${MATTER_LIB_DIR}.\n"
            "Attempting to use default Matter library paths.\n"
            "If build fails, ensure Matter SDK is built:\n"
            "  cd ${MATTER_ROOT}\n"
            "  ./scripts/build/build_examples.py --target linux-x64-all-clusters build")
        
        # Set generic library names and let linker find them
        set(MATTER_LIBS chip ChipDataModel)
    else()
        message(STATUS "Found Matter libraries in ${MATTER_LIB_DIR}")
        set(MATTER_LIBS ${CHIP_LIB} ${CHIP_APP_LIB})
    endif()
    
    link_directories(${MATTER_LIB_DIR})
else()
    message(STATUS "ENABLE_MATTER is OFF - building stub implementation")
    set(MATTER_LIBS "")
endif()

# Host bridge executable
add_executable(host_bridge
    main.cpp
    matter_bridge.cpp
    viking_bio_protocol_linux.c
)

# Include directories
target_include_directories(host_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(host_bridge
    pthread
    ${MATTER_LIBS}
)

# Add compile options
target_compile_options(host_bridge PRIVATE
    -Wall
    -Wextra
)

# Installation
install(TARGETS host_bridge
    RUNTIME DESTINATION bin
)

# Install systemd service file
install(FILES host_bridge.service
    DESTINATION /etc/systemd/system
    OPTIONAL
)

message(STATUS "Host bridge configuration complete")
if(ENABLE_MATTER)
    message(STATUS "  Matter support: ENABLED")
    message(STATUS "  Matter SDK: ${MATTER_ROOT}")
else()
    message(STATUS "  Matter support: DISABLED (stub implementation)")
    message(STATUS "  To enable: cmake -DENABLE_MATTER=ON")
endif()
