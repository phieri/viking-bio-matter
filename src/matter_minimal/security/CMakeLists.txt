cmake_minimum_required(VERSION 3.13)

# Matter Security Layer Library
# Provides PASE and session management

# Only build for Pico platform
if(NOT PICO_PLATFORM)
    message(STATUS "Skipping matter_security (not building for Pico)")
    return()
endif()

# Create matter_security library
add_library(matter_security STATIC
    pase.c
    session_mgr.c
)

# Include directories
target_include_directories(matter_security PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/matter_minimal/codec
    ${CMAKE_SOURCE_DIR}/platform/pico_w_chip_port
    ${CMAKE_SOURCE_DIR}/platform/pico_w_chip_port/config
)

# Link dependencies
target_link_libraries(matter_security
    pico_stdlib
    pico_time
    pico_rand
    pico_mbedtls
    matter_tlv
)

# Print configuration
message(STATUS "Matter Security Library configured")
message(STATUS "  - PASE (SPAKE2+) implementation")
message(STATUS "  - Session management with AES-128-CCM")
message(STATUS "  - Max sessions: 5")
