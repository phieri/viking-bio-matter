cmake_minimum_required(VERSION 3.13)

project(security_tests C)

# Only build tests when NOT targeting Pico platform
if(NOT PICO_PLATFORM)
    # Enable CTest
    enable_testing()
    
    # Get the security source directory
    get_filename_component(SECURITY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/matter_minimal/security" ABSOLUTE)
    get_filename_component(CODEC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/matter_minimal/codec" ABSOLUTE)
    
    # Add codec library (required by security layer)
    add_subdirectory(${CODEC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/codec)
    
    # Note: Cannot build full security layer for host tests due to Pico SDK dependencies
    # (pico_rand, pico_time, pico_mbedtls are Pico-only)
    # Tests would need to mock these dependencies
    
    message(STATUS "Security tests disabled - require Pico SDK dependencies")
    message(STATUS "  - pase.c requires: pico_rand, pico_time, mbedtls ECC")
    message(STATUS "  - session_mgr.c requires: pico_time, mbedtls CCM")
    message(STATUS "  - Run tests on actual Pico W hardware")
    
else()
    message(STATUS "Security tests disabled (Pico build - tests are host-only)")
endif()
