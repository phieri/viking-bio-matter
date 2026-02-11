cmake_minimum_required(VERSION 3.13)

# Matter support option (OFF by default)
option(ENABLE_MATTER "Enable Matter (connectedhomeip) support" OFF)

# Matter requires Pico W board
if(ENABLE_MATTER)
    if(NOT DEFINED PICO_BOARD OR PICO_BOARD STREQUAL "")
        set(PICO_BOARD pico_w CACHE STRING "Board type" FORCE)
        message(STATUS "Setting PICO_BOARD to pico_w (required for Matter)")
    elseif(NOT PICO_BOARD STREQUAL "pico_w")
        message(FATAL_ERROR "Matter support requires PICO_BOARD=pico_w (current: ${PICO_BOARD})")
    endif()
endif()

# Initialize the Pico SDK
include(pico_sdk_import.cmake)

project(viking_bio_matter C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Define source files
set(SOURCES
    src/main.c
    src/serial_handler.c
    src/matter_bridge.c
    src/viking_bio_protocol.c
)

# Add Matter platform sources if enabled
if(ENABLE_MATTER)
    message(STATUS "Matter support ENABLED")
    list(APPEND SOURCES
        platform/pico_w_chip_port/network_adapter.cpp
        platform/pico_w_chip_port/storage_adapter.cpp
        platform/pico_w_chip_port/crypto_adapter.cpp
        platform/pico_w_chip_port/platform_manager.cpp
    )
    add_compile_definitions(ENABLE_MATTER=1)
else()
    message(STATUS "Matter support DISABLED (use -DENABLE_MATTER=ON to enable)")
endif()

add_executable(viking_bio_matter ${SOURCES})

# Pull in common dependencies
target_link_libraries(viking_bio_matter
    pico_stdlib
    hardware_uart
    hardware_gpio
    hardware_sync
)

# Conditionally include Matter platform port
if(ENABLE_MATTER)
    # Matter build configuration
    set(CONFIG_CHIP_PROJECT_CONFIG ON)
    set(CONFIG_CHIP_ENABLE_WIFI ON)
    set(CONFIG_CHIP_ENABLE_ICD_SERVER OFF)
    set(CONFIG_CHIP_ENABLE_PAIRING_AUTOSTART ON)
    
    # Check for connectedhomeip submodule
    set(CHIP_ROOT "${CMAKE_SOURCE_DIR}/third_party/connectedhomeip")
    if(NOT EXISTS "${CHIP_ROOT}/src")
        message(FATAL_ERROR 
            "connectedhomeip submodule not found at ${CHIP_ROOT}\n"
            "Please run: git submodule update --init --recursive third_party/connectedhomeip\n"
            "Or clone with: git clone --recurse-submodules --branch v1.3-branch https://github.com/project-chip/connectedhomeip.git third_party/connectedhomeip")
    endif()
    
    target_include_directories(viking_bio_matter PRIVATE
        platform/pico_w_chip_port
        ${CHIP_ROOT}/src
        ${CHIP_ROOT}/src/include
        ${CHIP_ROOT}/src/lib
        ${CHIP_ROOT}/zzz_generated/app-common
    )
    
    # Link WiFi library which includes lwIP and mbedTLS with proper configuration
    target_link_libraries(viking_bio_matter
        pico_unique_id
        pico_cyw43_arch_lwip_threadsafe_background
        hardware_flash
    )
endif()

# Enable USB output, disable UART output
pico_enable_stdio_usb(viking_bio_matter 1)
pico_enable_stdio_uart(viking_bio_matter 0)

# Create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(viking_bio_matter)

target_include_directories(viking_bio_matter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
