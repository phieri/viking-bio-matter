# Matter Protocol Coordinator

add_library(matter_protocol STATIC
    matter_protocol.c
)

target_link_libraries(matter_protocol
    pico_stdlib
    matter_codec
    matter_transport
    matter_security
    matter_commissioning
    matter_interaction
    matter_clusters
)

target_include_directories(matter_protocol PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)
