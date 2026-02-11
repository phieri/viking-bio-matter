# Platform CMake for Pico W Matter (CHIP) port

if(NOT ENABLE_MATTER)
    message(STATUS "Matter support disabled (ENABLE_MATTER=OFF)")
    return()
endif()

message(STATUS "Configuring Matter (connectedhomeip) for Pico W...")

# Check for connectedhomeip submodule
set(CHIP_ROOT "${CMAKE_SOURCE_DIR}/third_party/connectedhomeip")
if(NOT EXISTS "${CHIP_ROOT}/src")
    message(FATAL_ERROR 
        "connectedhomeip submodule not found at ${CHIP_ROOT}\n"
        "Please run: git submodule update --init --recursive third_party/connectedhomeip\n"
        "Or clone with: git clone --recurse-submodules --branch v1.3-branch https://github.com/project-chip/connectedhomeip.git third_party/connectedhomeip")
endif()

# Matter build configuration
set(CONFIG_CHIP_PROJECT_CONFIG ON)
set(CONFIG_CHIP_ENABLE_WIFI ON)
set(CONFIG_CHIP_ENABLE_ICD_SERVER OFF)
set(CONFIG_CHIP_ENABLE_PAIRING_AUTOSTART ON)

# Platform sources
set(PICO_CHIP_PORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/network_adapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/storage_adapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto_adapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform_manager.cpp
)

# Create platform library as a static library (not INTERFACE)
# This ensures sources are compiled with proper include paths from linked libraries
add_library(pico_chip_port STATIC ${PICO_CHIP_PORT_SOURCES})

target_include_directories(pico_chip_port PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CHIP_ROOT}/src
    ${CHIP_ROOT}/src/include
    ${CHIP_ROOT}/src/lib
    ${CHIP_ROOT}/zzz_generated/app-common
)

# Link required Pico SDK libraries for Matter
target_link_libraries(pico_chip_port PUBLIC
    pico_stdlib
    pico_unique_id
    pico_cyw43_arch_lwip_threadsafe_background
    hardware_flash
    hardware_sync
)

message(STATUS "Matter platform port configured for Pico W")
