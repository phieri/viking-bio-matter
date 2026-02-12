cmake_minimum_required(VERSION 3.13)

project(tlv_tests C)

# Only build tests when NOT targeting Pico platform
if(NOT PICO_PLATFORM)
    # Enable CTest
    enable_testing()
    
    # Get the codec source directory
    get_filename_component(CODEC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/matter_minimal/codec" ABSOLUTE)
    
    # Add codec library subdirectory
    add_subdirectory(${CODEC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/codec)
    
    # Create test executable
    add_executable(test_tlv test_tlv.c)
    
    # Link to matter_tlv library
    target_link_libraries(test_tlv matter_tlv)
    
    # Add test to CTest
    add_test(NAME test_tlv COMMAND test_tlv)
    
    message(STATUS "TLV codec tests enabled (host build)")
else()
    message(STATUS "TLV codec tests disabled (Pico build)")
endif()
