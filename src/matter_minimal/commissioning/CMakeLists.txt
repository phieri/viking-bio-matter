# Commissioning Module

add_library(matter_commissioning STATIC
    network_commissioning.c
)

target_link_libraries(matter_commissioning
    pico_stdlib
    matter_security
    matter_codec
    pico_mbedtls
)

target_include_directories(matter_commissioning PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)
