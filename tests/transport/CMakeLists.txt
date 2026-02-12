cmake_minimum_required(VERSION 3.13)

project(transport_tests C)

# Only build tests when NOT targeting Pico platform
if(NOT PICO_PLATFORM)
    # Enable CTest
    enable_testing()
    
    # Get the codec source directory
    get_filename_component(CODEC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/matter_minimal/codec" ABSOLUTE)
    get_filename_component(TRANSPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/matter_minimal/transport" ABSOLUTE)
    
    # Add codec library subdirectory
    add_subdirectory(${CODEC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/codec)
    
    # Create test executable for message codec
    add_executable(test_message_codec test_message_codec.c)
    
    # Link to matter_tlv library (includes message_codec.c)
    target_link_libraries(test_message_codec matter_tlv)
    
    # Add test to CTest
    add_test(NAME test_message_codec COMMAND test_message_codec)
    
    # Create test executable for UDP transport
    # Note: UDP transport tests are limited on host without lwIP
    # They primarily test address parsing functions
    add_executable(test_udp_transport test_udp_transport.c)
    
    # Link to matter_tlv library
    target_link_libraries(test_udp_transport matter_tlv)
    
    # Include transport headers
    target_include_directories(test_udp_transport PRIVATE
        ${TRANSPORT_DIR}
        ${CODEC_DIR}
    )
    
    # Add test to CTest
    add_test(NAME test_udp_transport COMMAND test_udp_transport)
    
    message(STATUS "Matter transport tests enabled (host build)")
else()
    message(STATUS "Matter transport tests disabled (Pico build)")
endif()

