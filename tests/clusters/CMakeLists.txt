cmake_minimum_required(VERSION 3.13)

project(cluster_tests C)

# Only build tests when NOT targeting Pico platform
if(NOT PICO_PLATFORM)
    # Enable CTest
    enable_testing()
    
    # Get the source directories
    get_filename_component(CODEC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/matter_minimal/codec" ABSOLUTE)
    get_filename_component(INTERACTION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/matter_minimal/interaction" ABSOLUTE)
    get_filename_component(CLUSTERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/matter_minimal/clusters" ABSOLUTE)
    
    # Add subdirectories to build libraries
    add_subdirectory(${CODEC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/codec)
    add_subdirectory(${INTERACTION_DIR} ${CMAKE_CURRENT_BINARY_DIR}/interaction)
    add_subdirectory(${CLUSTERS_DIR} ${CMAKE_CURRENT_BINARY_DIR}/clusters)
    
    # Create test executable
    add_executable(test_clusters test_clusters.c)
    
    # Link to libraries
    target_link_libraries(test_clusters 
        matter_clusters
        matter_interaction
        matter_tlv
    )
    
    # Add test to CTest
    add_test(NAME test_clusters COMMAND test_clusters)
    
    message(STATUS "Cluster tests enabled (host build)")
else()
    message(STATUS "Cluster tests disabled (Pico build)")
endif()
