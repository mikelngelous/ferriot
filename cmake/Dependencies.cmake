# Dependencies.cmake
# Manages external dependencies using FetchContent

include(FetchContent)

# ============================================================================
# libcoap - CoAP implementation with DTLS support
# ============================================================================
find_package(libcoap-3 3.0 QUIET)

if(NOT libcoap-3_FOUND)
    message(STATUS "libcoap not found, fetching from source...")

    FetchContent_Declare(libcoap
        GIT_REPOSITORY https://github.com/obgm/libcoap.git
        GIT_TAG        v4.3.4
        GIT_SHALLOW    TRUE
    )

    # libcoap configuration
    set(ENABLE_DTLS ON CACHE BOOL "Enable DTLS support" FORCE)
    set(DTLS_BACKEND "openssl" CACHE STRING "DTLS backend" FORCE)
    set(ENABLE_EXAMPLES OFF CACHE BOOL "Build libcoap examples" FORCE)
    set(ENABLE_DOCS OFF CACHE BOOL "Build libcoap documentation" FORCE)
    set(ENABLE_TESTS OFF CACHE BOOL "Build libcoap tests" FORCE)

    # CMake 3.11+ compatible pattern (FetchContent_MakeAvailable requires 3.14+)
    FetchContent_GetProperties(libcoap)
    if(NOT libcoap_POPULATED)
        FetchContent_Populate(libcoap)
        add_subdirectory(${libcoap_SOURCE_DIR} ${libcoap_BINARY_DIR})
    endif()
else()
    message(STATUS "Found system libcoap: ${libcoap-3_VERSION}")
endif()

# Expose libcoap headers as SYSTEM so their C-style casts don't trip our -Werror
if(TARGET coap-3)
    get_target_property(_coap_incs coap-3 INTERFACE_INCLUDE_DIRECTORIES)
    if(_coap_incs)
        set_target_properties(coap-3 PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_coap_incs}")
    endif()
endif()

# ============================================================================
# OpenSSL - Required for DTLS
# ============================================================================
find_package(OpenSSL 1.1 REQUIRED)
message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")

# ============================================================================
# libcurl - HTTP/HTTPS downloads for FOTA
# ============================================================================
find_package(CURL REQUIRED)
message(STATUS "Found libcurl: ${CURL_VERSION_STRING}")

# ============================================================================
# GoogleTest - Testing framework (only if tests enabled)
# ============================================================================
if(LWM2M_BUILD_TESTS)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
        GIT_SHALLOW    TRUE
    )

    # Prevent GoogleTest from overriding compiler/linker options
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

    # CMake 3.11+ compatible pattern (FetchContent_MakeAvailable requires 3.14+)
    FetchContent_GetProperties(googletest)
    if(NOT googletest_POPULATED)
        FetchContent_Populate(googletest)
        add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
    endif()

    message(STATUS "GoogleTest configured")
endif()
