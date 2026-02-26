cmake_minimum_required(VERSION 3.13)
project(host_tools C)

# Build host-side tools (only when NOT targeting Pico)
if(PICO_PLATFORM)
    message(STATUS "Host tools skipped (Pico build)")
    return()
endif()

# ---------------------------------------------------------------
# attestation_verifier – verifies DAC→PAI→PAA chain + signature
# ---------------------------------------------------------------

# Try to find OpenSSL first; fall back to system mbedTLS.
find_package(OpenSSL QUIET)

if(OpenSSL_FOUND)
    message(STATUS "Host tools: using OpenSSL ${OPENSSL_VERSION}")
    add_executable(attestation_verifier attestation_verifier.c)
    target_link_libraries(attestation_verifier OpenSSL::SSL OpenSSL::Crypto)
else()
    # Try pkg-config for mbedTLS
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(MBEDTLS QUIET mbedtls mbedcrypto mbedx509)
    endif()

    if(MBEDTLS_FOUND)
        message(STATUS "Host tools: using mbedTLS (pkg-config)")
        add_executable(attestation_verifier attestation_verifier.c)
        target_compile_definitions(attestation_verifier PRIVATE USE_MBEDTLS)
        target_compile_options(attestation_verifier PRIVATE ${MBEDTLS_CFLAGS_OTHER})
        target_include_directories(attestation_verifier PRIVATE ${MBEDTLS_INCLUDE_DIRS})
        target_link_libraries(attestation_verifier ${MBEDTLS_LIBRARIES})
    else()
        message(WARNING "Host tools: neither OpenSSL nor mbedTLS found – "
                        "attestation_verifier will NOT be built. "
                        "Install libssl-dev or libmbedtls-dev.")
    endif()
endif()
