cmake_minimum_required(VERSION 3.13)

# Only build for Pico platform
if(NOT PICO_PLATFORM)
    message(STATUS "Skipping matter_transport (not building for Pico)")
    return()
endif()

# Create static library for Matter UDP transport
add_library(matter_transport STATIC
    udp_transport.c
)

# Link to matter_tlv (codec library) and Pico SDK libraries
target_link_libraries(matter_transport
    matter_tlv
    pico_stdlib
    pico_cyw43_arch_lwip_poll
)

# Set include directories
target_include_directories(matter_transport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../codec
    ${CMAKE_SOURCE_DIR}/platform/pico_w_chip_port/config
)

# Set C standard
set_property(TARGET matter_transport PROPERTY C_STANDARD 11)
