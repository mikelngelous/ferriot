# Examples CMakeLists.txt

add_executable(ferriot-client
    lwm2m_client.cpp
)

target_link_libraries(ferriot-client
    PRIVATE
        lwm2m
)

target_compile_options(ferriot-client PRIVATE ${LWM2M_WARNING_FLAGS})
