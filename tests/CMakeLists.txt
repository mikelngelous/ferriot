# Tests CMakeLists.txt

enable_testing()

# GoogleTest targets (GTest::gtest, ...) come from FetchContent in Dependencies.cmake
include(GoogleTest)

# ============================================================================
# Unit Tests
# ============================================================================

add_executable(lwm2m_tests
    test_types.cpp
    test_result.cpp
    test_tlv_codec.cpp
    test_cbor.cpp
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

# ============================================================================
# Integration Tests (optional, requires Leshan server running)
# ============================================================================

option(LWM2M_BUILD_INTEGRATION_TESTS "Build integration tests (requires CURL, jsoncpp, and running Leshan server)" OFF)

if(LWM2M_BUILD_INTEGRATION_TESTS)
    # Find required dependencies
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CURL REQUIRED libcurl)
    pkg_check_modules(JSONCPP REQUIRED jsoncpp)

    add_executable(lwm2m_integration_tests
        integration/leshan_api.cpp
        integration/test_leshan_integration.cpp
    )

    target_link_libraries(lwm2m_integration_tests
        PRIVATE
            lwm2m
            GTest::gtest
            GTest::gtest_main
            ${CURL_LIBRARIES}
            ${JSONCPP_LIBRARIES}
    )

    target_include_directories(lwm2m_integration_tests
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/integration
            ${CURL_INCLUDE_DIRS}
            ${JSONCPP_INCLUDE_DIRS}
    )

    target_compile_options(lwm2m_integration_tests PRIVATE ${LWM2M_WARNING_FLAGS})

    # Register integration tests (but they will skip if Leshan is not running)
    gtest_discover_tests(lwm2m_integration_tests
        TEST_PREFIX "integration."
        PROPERTIES LABELS "integration"
    )
endif()
