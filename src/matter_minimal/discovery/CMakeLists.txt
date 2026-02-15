# DNS-SD discovery module for Matter
add_library(matter_discovery INTERFACE)

target_sources(matter_discovery INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/dns_sd.c
)

target_include_directories(matter_discovery INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(matter_discovery INTERFACE
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
)
