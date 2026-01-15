# Tests CMakeLists.txt

enable_testing()

# Find GoogleTest
find_package(GTest REQUIRED)
include(GoogleTest)

# Test executable
add_executable(lwm2m_tests
    test_types.cpp
    test_result.cpp
    test_tlv_codec.cpp
    test_objects.cpp
)

target_link_libraries(lwm2m_tests
    PRIVATE
        lwm2m
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(lwm2m_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Enable warnings for tests too
target_compile_options(lwm2m_tests PRIVATE ${LWM2M_WARNING_FLAGS})

# Discover tests
gtest_discover_tests(lwm2m_tests)
