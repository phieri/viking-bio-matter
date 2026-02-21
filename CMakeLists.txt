cmake_minimum_required(VERSION 3.17)

# This firmware requires Pico W for Matter/WiFi support
if(NOT DEFINED PICO_BOARD OR PICO_BOARD STREQUAL "")
    set(PICO_BOARD pico_w CACHE STRING "Board type" FORCE)
    message(STATUS "Setting PICO_BOARD to pico_w (required for Matter)")
elseif(NOT PICO_BOARD STREQUAL "pico_w")
    message(FATAL_ERROR "This firmware requires PICO_BOARD=pico_w (current: ${PICO_BOARD})")
endif()

# Set custom mbedTLS configuration (SDK 2.x) - must be before pico_sdk_import.cmake
# Define MBEDTLS_ALLOW_PRIVATE_ACCESS to enable access to private members in mbedTLS 3.x
add_compile_definitions(MBEDTLS_ALLOW_PRIVATE_ACCESS)
set(PICO_MBEDTLS_CONFIG_HEADER "${CMAKE_CURRENT_LIST_DIR}/platform/pico_w_chip_port/config/mbedtls_config.h" CACHE STRING "Custom mbedTLS config" FORCE)

# Place BTstack flash bank (8KB) immediately before the LittleFS storage area
# (last 256KB of flash) to prevent overlap.  Without this, BTstack defaults to
# the very end of flash (2MB - 8KB = 0x1FE000), which falls INSIDE the LittleFS
# region (0x1C0000-0x200000).  When cyw43_arch_init() calls btstack_cyw43_init()
# -> setup_tlv() -> btstack_tlv_flash_bank_init_instance(), the TLV scanner reads
# LittleFS data from 0x1FE000.  A record 'len' field in that data can cause a
# uint32_t offset overflow in the scan loop, producing an infinite loop and the
# startup hang seen at "Initializing CYW43439 WiFi adapter...".
#
# MUST use add_compile_definitions() (global, before pico_sdk_init) so the
# definition is visible when the SDK's pico_btstack_flash_bank.c is compiled.
# target_compile_definitions(... PRIVATE ...) would only apply to our own sources.
add_compile_definitions(
    "PICO_FLASH_BANK_STORAGE_OFFSET=(PICO_FLASH_SIZE_BYTES - (256 * 1024) - (FLASH_SECTOR_SIZE * 2))"
)

# Initialize the Pico SDK
include(pico_sdk_import.cmake)

project(viking_bio_matter C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# ============================================
# Build Version Information
# ============================================
# Get git information for versioning
execute_process(
    COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# If git describe fails, try to get just the commit hash
if(NOT GIT_VERSION)
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_COMMIT)
        set(GIT_VERSION "${GIT_COMMIT}")
    else()
        set(GIT_VERSION "unknown")
    endif()
endif()

# Get current timestamp
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S UTC" UTC)

# Set firmware version
set(FIRMWARE_VERSION "${GIT_VERSION}")

message(STATUS "Firmware Version: ${FIRMWARE_VERSION}")
message(STATUS "Build Timestamp: ${BUILD_TIMESTAMP}")

# Pass version info as compile definitions
add_compile_definitions(
    FIRMWARE_VERSION="${FIRMWARE_VERSION}"
    BUILD_TIMESTAMP="${BUILD_TIMESTAMP}"
    GIT_COMMIT_HASH="${GIT_VERSION}"
)
# ============================================

# Initialize the Pico SDK
pico_sdk_init()

# Include pico-lfs library for LittleFS support
add_subdirectory(libs/pico-lfs)

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
    src/version.c
    src/serial_handler.c
    src/matter_bridge.cpp
    src/viking_bio_protocol.c
    src/multicore_coordinator.c
    platform/pico_w_chip_port/network_adapter.cpp
    platform/pico_w_chip_port/storage_adapter.cpp
    platform/pico_w_chip_port/crypto_adapter.cpp
    platform/pico_w_chip_port/platform_manager.cpp
    platform/pico_w_chip_port/matter_attributes.cpp
    platform/pico_w_chip_port/matter_reporter.cpp
    platform/pico_w_chip_port/matter_network_transport.cpp
    platform/pico_w_chip_port/matter_network_subscriber.cpp
    platform/pico_w_chip_port/ble_adapter.cpp
)

# Add lwIP mDNS source files (not compiled by default in Pico SDK)
set(LWIP_MDNS_SOURCES
    ${PICO_SDK_PATH}/lib/lwip/src/apps/mdns/mdns.c
    ${PICO_SDK_PATH}/lib/lwip/src/apps/mdns/mdns_out.c
    ${PICO_SDK_PATH}/lib/lwip/src/apps/mdns/mdns_domain.c
)

# Matter is always enabled
add_compile_definitions(ENABLE_MATTER=1)
message(STATUS "Building with Matter support for Pico W")

add_executable(viking_bio_matter ${SOURCES} ${LWIP_MDNS_SOURCES})

target_include_directories(viking_bio_matter PRIVATE
    platform/pico_w_chip_port/config
    platform/pico_w_chip_port
    src/matter_minimal/codec
    src/matter_minimal/transport
    src/matter_minimal/security
    src/matter_minimal/commissioning
    src/matter_minimal/interaction
    src/matter_minimal/clusters
    src/matter_minimal/discovery
)

# Pull in all dependencies
target_link_libraries(viking_bio_matter
    pico_stdlib
    hardware_uart
    hardware_gpio
    hardware_sync
    pico_multicore
    pico_unique_id
    pico_cyw43_arch_lwip_threadsafe_background
    pico_mbedtls
    hardware_flash
    pico-lfs
    pico_btstack_ble
    pico_btstack_cyw43
    -Wl,--start-group
    matter_protocol
    matter_tlv
    matter_transport
    matter_security
    matter_commissioning
    matter_interaction
    matter_clusters
    matter_discovery
    -Wl,--end-group
)

# LittleFS compile definitions
target_compile_definitions(viking_bio_matter PRIVATE
    LFS_THREADSAFE=1
    LFS_NO_DEBUG=1
)

# Enable USB output, disable UART output
pico_enable_stdio_usb(viking_bio_matter 1)
pico_enable_stdio_uart(viking_bio_matter 0)

# Create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(viking_bio_matter)

# Rename output files to include version
add_custom_command(TARGET viking_bio_matter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rename
        ${CMAKE_BINARY_DIR}/viking_bio_matter.uf2
        ${CMAKE_BINARY_DIR}/viking_bio_matter-${FIRMWARE_VERSION}.uf2
    COMMAND ${CMAKE_COMMAND} -E rename
        ${CMAKE_BINARY_DIR}/viking_bio_matter.elf
        ${CMAKE_BINARY_DIR}/viking_bio_matter-${FIRMWARE_VERSION}.elf
    COMMAND ${CMAKE_COMMAND} -E rename
        ${CMAKE_BINARY_DIR}/viking_bio_matter.bin
        ${CMAKE_BINARY_DIR}/viking_bio_matter-${FIRMWARE_VERSION}.bin
    COMMAND ${CMAKE_COMMAND} -E rename
        ${CMAKE_BINARY_DIR}/viking_bio_matter.hex
        ${CMAKE_BINARY_DIR}/viking_bio_matter-${FIRMWARE_VERSION}.hex
    COMMENT "Renaming firmware files to include version: viking_bio_matter-${FIRMWARE_VERSION}.*"
)

target_include_directories(viking_bio_matter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add tests directory (only built for host, not Pico)
add_subdirectory(tests/codec)
add_subdirectory(tests/transport)
add_subdirectory(tests/security)
add_subdirectory(tests/interaction)
add_subdirectory(tests/clusters)
add_subdirectory(tests/storage)

# Add matter_minimal subdirectories for Pico build
if(PICO_PLATFORM)
    add_subdirectory(src/matter_minimal)
    add_subdirectory(src/matter_minimal/codec)
    add_subdirectory(src/matter_minimal/transport)
    add_subdirectory(src/matter_minimal/security)
    add_subdirectory(src/matter_minimal/commissioning)
    add_subdirectory(src/matter_minimal/interaction)
    add_subdirectory(src/matter_minimal/clusters)
    add_subdirectory(src/matter_minimal/discovery)
endif()

