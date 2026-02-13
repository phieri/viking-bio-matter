cmake_minimum_required(VERSION 3.13)

# This firmware requires Pico W for Matter/WiFi support
if(NOT DEFINED PICO_BOARD OR PICO_BOARD STREQUAL "")
    set(PICO_BOARD pico_w CACHE STRING "Board type" FORCE)
    message(STATUS "Setting PICO_BOARD to pico_w (required for Matter)")
elseif(NOT PICO_BOARD STREQUAL "pico_w")
    message(FATAL_ERROR "This firmware requires PICO_BOARD=pico_w (current: ${PICO_BOARD})")
endif()

# Initialize the Pico SDK
include(pico_sdk_import.cmake)

project(viking_bio_matter C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Compiler optimizations - prioritize speed over size
# -O3: Aggressive optimization for speed
# -ffast-math: Fast floating point math (safe for this application)
# -fno-signed-zeros: Allow optimizations assuming no signed zeros
# -fno-trapping-math: Allow optimizations assuming no FP exceptions
# -funroll-loops: Unroll loops for speed
add_compile_options(
    -O3
    -ffast-math
    -fno-signed-zeros
    -fno-trapping-math
    -funroll-loops
)

# Note: LTO disabled due to Pico SDK compatibility issues with wrapped functions
# set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# Define source files (Matter platform included)
set(SOURCES
    src/main.c
    src/serial_handler.c
    src/matter_bridge.cpp
    src/viking_bio_protocol.c
    platform/pico_w_chip_port/network_adapter.cpp
    platform/pico_w_chip_port/storage_adapter.cpp
    platform/pico_w_chip_port/crypto_adapter.cpp
    platform/pico_w_chip_port/platform_manager.cpp
    platform/pico_w_chip_port/matter_attributes.cpp
    platform/pico_w_chip_port/matter_reporter.cpp
    platform/pico_w_chip_port/matter_network_transport.cpp
    platform/pico_w_chip_port/matter_network_subscriber.cpp
)

# Matter is always enabled
add_compile_definitions(ENABLE_MATTER=1)
message(STATUS "Building with Matter support for Pico W")

add_executable(viking_bio_matter ${SOURCES})

target_include_directories(viking_bio_matter PRIVATE
    platform/pico_w_chip_port/config
    platform/pico_w_chip_port
    src/matter_minimal/codec
    src/matter_minimal/transport
    src/matter_minimal/security
    src/matter_minimal/commissioning
    src/matter_minimal/interaction
    src/matter_minimal/clusters
)

# Pull in all dependencies
target_link_libraries(viking_bio_matter
    pico_stdlib
    hardware_uart
    hardware_gpio
    hardware_sync
    pico_unique_id
    pico_cyw43_arch_lwip_threadsafe_background
    pico_mbedtls
    hardware_flash
    matter_protocol
    matter_tlv
    matter_transport
    matter_security
    matter_commissioning
    matter_interaction
    matter_clusters
)

# Enable USB output, disable UART output
pico_enable_stdio_usb(viking_bio_matter 1)
pico_enable_stdio_uart(viking_bio_matter 0)

# Create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(viking_bio_matter)

target_include_directories(viking_bio_matter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add tests directory (only built for host, not Pico)
add_subdirectory(tests/codec)
add_subdirectory(tests/transport)
add_subdirectory(tests/security)
add_subdirectory(tests/interaction)
add_subdirectory(tests/clusters)

# Add matter_minimal subdirectories for Pico build
if(PICO_PLATFORM)
    add_subdirectory(src/matter_minimal)
    add_subdirectory(src/matter_minimal/codec)
    add_subdirectory(src/matter_minimal/transport)
    add_subdirectory(src/matter_minimal/security)
    add_subdirectory(src/matter_minimal/commissioning)
    add_subdirectory(src/matter_minimal/interaction)
    add_subdirectory(src/matter_minimal/clusters)
endif()

