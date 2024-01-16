cmake_minimum_required(VERSION 3.15)

project(async-mqtt5-tests CXX)

include(../cmake/project-is-top-level.cmake)

if(PROJECT_IS_TOP_LEVEL)
  find_package(async-mqtt5 REQUIRED)
  enable_testing()
endif()

file(GLOB integration_tests "integration/*.cpp")
file(GLOB unit_tests "unit/*.cpp")

add_executable(mqtt-test src/run_tests.cpp ${integration_tests} ${unit_tests})

target_include_directories(mqtt-test PRIVATE include)
target_compile_definitions(mqtt-test PRIVATE BOOST_TEST_NO_MAIN=1)

find_package(OpenSSL REQUIRED)
target_link_libraries(mqtt-test PRIVATE Async::MQTT5 OpenSSL::SSL)

add_test(NAME mqtt-test COMMAND mqtt-test)
