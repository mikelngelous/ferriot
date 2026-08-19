cmake_minimum_required(VERSION 3.12)

project(ferriot
    VERSION 0.1.0
    DESCRIPTION "Ferriot - Modern C++17 LWM2M client for Linux edge"
    LANGUAGES CXX
)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(LWM2M_BUILD_TESTS "Build unit tests" ON)
option(LWM2M_BUILD_EXAMPLES "Build examples" ON)
option(LWM2M_ENABLE_SANITIZERS "Enable ASAN/UBSAN in Debug builds" OFF)

# Include custom cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Compiler warnings
include(CompilerWarnings)

# Dependencies (libcoap, OpenSSL, GoogleTest)
include(Dependencies)

# Threads support (required for std::thread)
find_package(Threads REQUIRED)

# Main library
add_library(lwm2m
    src/types.cpp
    src/resource.cpp
    src/object.cpp
    src/codec/tlv.cpp
    src/codec/cbor.cpp
    src/transport/coap_client.cpp
    src/objects/security.cpp
    src/objects/server.cpp
    src/objects/device.cpp
    src/objects/location.cpp
    src/objects/firmware_update.cpp
    src/objects/connectivity.cpp
    src/client.cpp
)

# Create alias for consistency
add_library(Ferriot::Ferriot ALIAS lwm2m)

target_include_directories(lwm2m
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(lwm2m
    PUBLIC
        coap-3
        OpenSSL::SSL
        OpenSSL::Crypto
        CURL::libcurl
        Threads::Threads
)

# Apply warnings to our library
target_compile_options(lwm2m PRIVATE ${LWM2M_WARNING_FLAGS})

# Sanitizers for Debug builds
if(LWM2M_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(lwm2m PUBLIC -fsanitize=address,undefined)
    target_link_options(lwm2m PUBLIC -fsanitize=address,undefined)
endif()

# Tests
if(LWM2M_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(LWM2M_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install
install(TARGETS lwm2m
    EXPORT lwm2m-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/lwm2m
    DESTINATION include
)
