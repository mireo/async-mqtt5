cmake_minimum_required(VERSION 3.15)

project(async-mqtt5-examples CXX)

include(../cmake/project-is-top-level.cmake)

if(PROJECT_IS_TOP_LEVEL)
  find_package(async-mqtt5 REQUIRED)
endif()

function(add_example name)
  add_executable("${name}" ${ARGN})
  target_compile_features("${name}" PRIVATE cxx_std_20) # for coroutines
  target_link_libraries("${name}" PRIVATE Async::MQTT5)
endfunction()

 foreach(f publisher receiver)
   add_example("${f}" "${f}.cpp")
 endforeach()

set(EXAMPLES
    tcp.cpp
    openssl_tls.cpp
    websocket_tcp.cpp
    websocket_tls.cpp
)

find_package(OpenSSL REQUIRED)
add_executable(examples src/run_examples.cpp ${EXAMPLES})
target_compile_features(examples PRIVATE cxx_std_17)
target_link_libraries(examples PRIVATE Async::MQTT5 OpenSSL::SSL)
