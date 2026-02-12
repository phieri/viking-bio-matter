cmake_minimum_required(VERSION 3.13)

# Create static library for Matter TLV codec
add_library(matter_tlv STATIC
    tlv.c
    message_codec.c
)

# Set include directories
target_include_directories(matter_tlv PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set C standard
set_property(TARGET matter_tlv PROPERTY C_STANDARD 11)
