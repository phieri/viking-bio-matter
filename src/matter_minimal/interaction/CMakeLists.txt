cmake_minimum_required(VERSION 3.13)

# Create static library for Matter Interaction Model
add_library(matter_interaction STATIC
    read_handler.c
    subscribe_handler.c
    report_generator.c
    subscription_bridge.cpp
)

# Set include directories
target_include_directories(matter_interaction PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Link to TLV codec library
target_link_libraries(matter_interaction
    matter_tlv
)

# Set C standard
set_property(TARGET matter_interaction PROPERTY C_STANDARD 11)
