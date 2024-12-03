cmake_minimum_required(VERSION 3.15)

project(async-mqtt5 VERSION 1.0.2 LANGUAGES CXX)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

add_library(async_mqtt5 INTERFACE)
add_library(Async::MQTT5 ALIAS async_mqtt5)

set_property(
    TARGET async_mqtt5 PROPERTY
    EXPORT_NAME MQTT5
)

target_include_directories(
    async_mqtt5 ${warning_guard}
    INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_compile_features(async_mqtt5 INTERFACE cxx_std_17)

find_package(Boost 1.82 REQUIRED)
target_link_libraries(async_mqtt5
    INTERFACE
    Boost::headers
)

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

if(PROJECT_IS_TOP_LEVEL)
  option(BUILD_EXAMPLES "Build examples tree." "${async-mqtt5_DEVELOPER_MODE}")
  if(BUILD_EXAMPLES)
    add_subdirectory(example)
  endif()
endif()

if(NOT async-mqtt5_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of async-mqtt5"
  )
endif()

include(cmake/dev-mode.cmake)
