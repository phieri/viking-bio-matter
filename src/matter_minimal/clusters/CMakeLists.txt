cmake_minimum_required(VERSION 3.13)

# Create static library for Matter Clusters
add_library(matter_clusters STATIC
    descriptor.c
    onoff.c
    level_control.c
    temperature.c
    network_commissioning.c
    diagnostics.c
)

# Set include directories
target_include_directories(matter_clusters PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Link to interaction model
target_link_libraries(matter_clusters
    matter_interaction
)

# Set C standard
set_property(TARGET matter_clusters PROPERTY C_STANDARD 11)
