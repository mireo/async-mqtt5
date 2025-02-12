#
# Copyright (c) 2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
#

project(boost_mqtt5_tests CXX)

file(GLOB integration_tests "integration/*.cpp")
file(GLOB unit_tests "unit/*.cpp")

add_executable(mqtt-test src/run_tests.cpp ${integration_tests} ${unit_tests})

target_include_directories(mqtt-test PRIVATE include)
target_compile_definitions(mqtt-test PRIVATE BOOST_TEST_NO_MAIN=1)

if(BOOST_MQTT5_MAIN_PROJECT)
  find_package(OpenSSL REQUIRED)
  target_compile_definitions(mqtt-test PRIVATE BOOST_MQTT5_EXTRA_DEPS=1)

  target_link_libraries(
    mqtt-test PRIVATE
    Boost::mqtt5
    OpenSSL::SSL
  )
else()
  target_link_libraries(
    mqtt-test PRIVATE
    Boost::mqtt5
    Boost::included_unit_test_framework
  )

  # Follow the Boost convention: don't build test targets by default,
  # and only when explicitly requested by building target tests
  set_target_properties(mqtt-test PROPERTIES EXCLUDE_FROM_ALL ON)
  add_dependencies(tests mqtt-test)
endif()

include(CTest)
add_test(NAME mqtt-test COMMAND mqtt-test)

if (BOOST_MQTT5_PUBLIC_BROKER_TESTS)
  set_property(TEST mqtt-test PROPERTY ENVIRONMENT "BOOST_MQTT5_PUBLIC_BROKER_TESTS=1")
endif()
