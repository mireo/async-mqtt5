cmake_minimum_required(VERSION 3.15)

project(async-mqtt5-examples CXX)

include(../cmake/project-is-top-level.cmake)

if(PROJECT_IS_TOP_LEVEL)
  find_package(async-mqtt5 REQUIRED)
endif()

function(add_example name)
  add_executable("${name}" ${ARGN})
  target_link_libraries("${name}" PRIVATE Async::MQTT5)
  string(FIND "${example_name}" "tls" found_tls)
  if(found_tls GREATER -1)
    target_link_libraries("${name}" PRIVATE OpenSSL::SSL)
  endif()
endfunction()

file(GLOB examples "*.cpp")

foreach(file_path ${examples})
  get_filename_component(example_name "${file_path}" NAME_WE)
  add_example("${example_name}" "${file_path}")
endforeach()

find_package(OpenSSL REQUIRED)
