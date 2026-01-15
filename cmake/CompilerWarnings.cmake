# CompilerWarnings.cmake
# Strict compiler warnings for clean code

set(LWM2M_WARNING_FLAGS "")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    list(APPEND LWM2M_WARNING_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
    )

    # GCC specific warnings
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        list(APPEND LWM2M_WARNING_FLAGS
            -Wmisleading-indentation
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
            -Wuseless-cast
        )
    endif()

    # Clang specific warnings
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        list(APPEND LWM2M_WARNING_FLAGS
            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic
        )
    endif()

elseif(MSVC)
    list(APPEND LWM2M_WARNING_FLAGS
        /W4
        /WX
        /permissive-
    )
endif()

message(STATUS "Compiler warnings: ${LWM2M_WARNING_FLAGS}")
