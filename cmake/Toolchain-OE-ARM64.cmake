# Toolchain file for cross-compiling to ARM64 using OE SDK
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-OE-ARM64.cmake ..

# OE SDK root (adjust if different)
set(OE_SDK_ROOT "/usr/local/oecore-x86_64" CACHE PATH "OE SDK root")

# Validate SDK exists
if(NOT EXISTS "${OE_SDK_ROOT}")
    message(FATAL_ERROR "OE SDK not found at ${OE_SDK_ROOT}")
endif()

# Target system
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Sysroot
set(CMAKE_SYSROOT "${OE_SDK_ROOT}/sysroots/aarch64-oe-linux")
set(CMAKE_STAGING_PREFIX "${CMAKE_SYSROOT}")

# Cross compiler paths
set(CROSS_COMPILE "${OE_SDK_ROOT}/sysroots/x86_64-oesdk-linux/usr/bin/aarch64-oe-linux/aarch64-oe-linux-")

set(CMAKE_C_COMPILER "${CROSS_COMPILE}gcc")
set(CMAKE_CXX_COMPILER "${CROSS_COMPILE}g++")
set(CMAKE_AR "${CROSS_COMPILE}ar")
set(CMAKE_RANLIB "${CROSS_COMPILE}ranlib")
set(CMAKE_STRIP "${CROSS_COMPILE}strip")

# Search paths
set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Compiler flags from OE environment
set(CMAKE_C_FLAGS_INIT "-O2 -pipe -g -feliminate-unused-debug-types")
set(CMAKE_CXX_FLAGS_INIT "-O2 -pipe -g -feliminate-unused-debug-types")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed")

# PKG_CONFIG for cross-compilation
set(ENV{PKG_CONFIG_PATH} "")
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")

message(STATUS "Cross-compiling for ARM64 using OE SDK at ${OE_SDK_ROOT}")
message(STATUS "Sysroot: ${CMAKE_SYSROOT}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "CXX Compiler: ${CMAKE_CXX_COMPILER}")
