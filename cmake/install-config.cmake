include(CMakeFindDependencyMacro)
find_dependency(Boost 1.82)

include("${CMAKE_CURRENT_LIST_DIR}/async-mqtt5Targets.cmake")
