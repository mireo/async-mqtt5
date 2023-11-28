cmake_minimum_required(VERSION 3.15)

project(async-mqtt5-tests CXX)

include(../../cmake/project-is-top-level.cmake)

if(PROJECT_IS_TOP_LEVEL)
  find_package(async_mqtt5 REQUIRED)
  enable_testing()
endif()

add_executable(
    mqtt-test
    src/run_tests.cpp
    test/cancellation.cpp
    test/client_broker.cpp
    test/coroutine.cpp
    test/publish_send_op.cpp
    test/serialization.cpp
    test/session.cpp
)

target_include_directories(mqtt-test PRIVATE include)
target_compile_features(mqtt-test PRIVATE cxx_std_20)

find_package(Boost 1.82 REQUIRED unit_test_framework)
target_link_libraries(mqtt-test PRIVATE Async::MQTT5 Boost::unit_test_framework)

add_test(NAME mqtt-test COMMAND mqtt-test)
