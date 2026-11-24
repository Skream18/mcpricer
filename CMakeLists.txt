cmake_minimum_required(VERSION 3.24)
project(mcpricer VERSION 0.1.0 LANGUAGES CXX)

option(MCPRICER_BUILD_TESTS "Build the unit tests (downloads GoogleTest)" ON)
option(MCPRICER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(MCPRICER_SANITIZER "" CACHE STRING "Sanitizer to enable: address, thread, or empty")

# Single-config generators default to an empty build type, i.e. no optimisation.
get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT is_multi_config AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/CompilerSettings.cmake)

add_library(mcpricer_core STATIC
  src/analytic.cpp
  src/market.cpp
  src/version.cpp
)
target_include_directories(mcpricer_core PUBLIC include)
target_compile_definitions(mcpricer_core PRIVATE MCPRICER_VERSION="${PROJECT_VERSION}")
mcpricer_set_warnings(mcpricer_core)

if(MCPRICER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
