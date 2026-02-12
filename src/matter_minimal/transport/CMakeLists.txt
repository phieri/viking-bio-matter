cmake_minimum_required(VERSION 3.13)

# Create static library for Matter UDP transport
add_library(matter_transport STATIC
    udp_transport.c
)

# Link to matter_tlv (codec library)
target_link_libraries(matter_transport
    matter_tlv
)

# Set include directories
target_include_directories(matter_transport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../codec
)

# Set C standard
set_property(TARGET matter_transport PROPERTY C_STANDARD 11)

# Note: lwIP headers are provided by pico_cyw43_arch_lwip_threadsafe_background
# which is linked in the main executable
