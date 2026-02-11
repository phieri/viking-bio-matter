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

# Conditionally include Matter platform port
if(ENABLE_MATTER)
    message(STATUS "Matter support ENABLED")
    add_subdirectory(platform/pico_w_chip_port)
    add_compile_definitions(ENABLE_MATTER=1)
else()
    message(STATUS "Matter support DISABLED (use -DENABLE_MATTER=ON to enable)")
endif()

# Link Matter platform libraries if enabled
if(ENABLE_MATTER)
    target_link_libraries(viking_bio_matter
        pico_cyw43_arch_lwip_threadsafe_background
        pico_chip_port
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
