cmake_minimum_required(VERSION 3.13)

project(storage_tests C)

# Storage tests require Pico hardware (LittleFS with flash memory)
# They cannot run on host, so we provide them as example code
# that can be included in the main firmware for manual testing

if(NOT PICO_PLATFORM)
    message(STATUS "Storage tests disabled (require Pico hardware)")
else()
    message(STATUS "Storage test code available for manual testing on Pico")
    # Tests are not automatically built but can be included in main firmware
    # by adding test_storage_adapter.c to the main sources and calling
    # run_all_storage_tests() from main()
endif()
