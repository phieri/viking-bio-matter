cmake_minimum_required(VERSION 3.13)

project(interaction_tests C)

# Only build tests when NOT targeting Pico platform
if(NOT PICO_PLATFORM)
    # Enable CTest
    enable_testing()
    
    # Get the source directories
    get_filename_component(CODEC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/matter_minimal/codec" ABSOLUTE)
    get_filename_component(INTERACTION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/matter_minimal/interaction" ABSOLUTE)
    
    # Add subdirectories to build libraries
    add_subdirectory(${CODEC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/codec)
    add_subdirectory(${INTERACTION_DIR} ${CMAKE_CURRENT_BINARY_DIR}/interaction)
    
    # Create test executables
    add_executable(test_read_handler test_read_handler.c)
    add_executable(test_subscribe_handler test_subscribe_handler.c)
    add_executable(test_report_generator test_report_generator.c)
    
    # Link to libraries
    target_link_libraries(test_read_handler 
        matter_interaction
        matter_tlv
    )
    
    target_link_libraries(test_subscribe_handler
        matter_interaction
        matter_tlv
    )
    
    target_link_libraries(test_report_generator
        matter_interaction
        matter_tlv
    )
    
    # Add tests to CTest
    add_test(NAME test_read_handler COMMAND test_read_handler)
    add_test(NAME test_subscribe_handler COMMAND test_subscribe_handler)
    add_test(NAME test_report_generator COMMAND test_report_generator)
    
    message(STATUS "Interaction model tests enabled (host build)")
else()
    message(STATUS "Interaction model tests disabled (Pico build)")
endif()
