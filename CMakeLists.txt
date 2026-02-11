cmake_minimum_required(VERSION 3.13)

# Option to enable host-side Matter bridge (Raspberry Pi Zero)
option(ENABLE_MATTER "Enable building host-side Matter bridge" OFF)

# Only initialize Pico SDK if not building host bridge only
if(NOT ENABLE_MATTER)
    # Initialize the Pico SDK
    include(pico_sdk_import.cmake)
endif()

project(viking_bio_matter C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Build Pico firmware (default) or host bridge
if(NOT ENABLE_MATTER)
    # ===== Raspberry Pi Pico Firmware Build =====
    message(STATUS "ENABLE_MATTER is OFF - building Raspberry Pi Pico firmware (default)")
    
    # Initialize the Pico SDK
    pico_sdk_init()

    add_executable(viking_bio_matter
        src/main.c
        src/serial_handler.c
        src/matter_bridge.c
        src/viking_bio_protocol.c
    )

    # Pull in common dependencies
    target_link_libraries(viking_bio_matter
        pico_stdlib
        hardware_uart
        hardware_gpio
        hardware_sync
    )

    # Enable USB output, disable UART output
    pico_enable_stdio_usb(viking_bio_matter 1)
    pico_enable_stdio_uart(viking_bio_matter 0)

    # Create map/bin/hex/uf2 file etc.
    pico_add_extra_outputs(viking_bio_matter)

    target_include_directories(viking_bio_matter PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
else()
    # ===== Raspberry Pi Zero Host Bridge Build =====
    message(STATUS "ENABLE_MATTER is ON - building host bridge for Raspberry Pi Zero")
    add_subdirectory(host_bridge)
endif()
