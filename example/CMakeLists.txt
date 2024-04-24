cmake_minimum_required(VERSION 3.15)

project(async-mqtt5-examples CXX)

include(../cmake/project-is-top-level.cmake)

if(PROJECT_IS_TOP_LEVEL)
  find_package(async-mqtt5 REQUIRED)
endif()

# set(EXAMPLE <your-source-file>.cpp)
set(EXAMPLE hello_world_over_tcp.cpp)

find_package(OpenSSL REQUIRED) # if you require SSL connection 
add_executable(example ${EXAMPLE})
target_compile_features(example PRIVATE cxx_std_17) # or cxx_std_20
target_link_libraries(example PRIVATE Async::MQTT5 OpenSSL::SSL)
