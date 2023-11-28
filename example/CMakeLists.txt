cmake_minimum_required(VERSION 3.15)

project(async-mqtt5-examples CXX)

include(../cmake/project-is-top-level.cmake)

if(PROJECT_IS_TOP_LEVEL)
  find_package(async_mqtt5 REQUIRED)
endif()

function(add_example name)
  add_executable("${name}" ${ARGN})
  target_compile_features("${name}" PRIVATE cxx_std_20)
  target_link_libraries("${name}" PRIVATE Async::MQTT5)
endfunction()

foreach(f callbacks cpp20_coroutines futures publisher receiver)
  add_example("${f}" "${f}.cpp")
endforeach()

#[[
add_example(
    misc
    src/run_examples.cpp
    network_connection.cpp
    openssl-tls.cpp
    tcp.cpp
    websocket-tcp.cpp
    websocket-tls.cpp
)

find_package(OpenSSL REQUIRED)
target_link_libraries(misc PRIVATE OpenSSL::SSL)
]]
