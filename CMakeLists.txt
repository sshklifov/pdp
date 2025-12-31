cmake_minimum_required(VERSION 3.12)

project(pdp VERSION 1.0.0 DESCRIPTION "GDB/MI parsing and command processing library")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -O0 -ggdb")
# TODO: Future: Remove debugging information
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g1 -fno-omit-frame-pointer -O3 -Wall")

# Build unit tests
option(BUILD_TESTS "Build unit tests" ON)

set(_FEATURES
  # Use tracing allocator and emit allocation statistics"
  TRACE_ALLOCATIONS
  # Enable pthread error-checking mutexes 
  CHECK_MUTEX
  # Enable runtime/assert checks 
  ENABLE_ASSERT
  # Enables pdp_trace and pdp_trace_once debug messages 
  TRACE_MESSAGES
  # Report branch mispredictions originating from PDP_TRACE_LIKELY 
  TRACE_BRANCH
  # Allow use of external, unverified code 
  ENABLE_EXTERNAL_CODE
  # Trace RollingBuffer reallocation statistics 
  TRACE_ROLLING_BUFFER
)

# TODO Feature: ENABLE_BACKTRACE

# TODO: Experiment with LTO and add_library stuff

set(_DEBUG_ENABLED_FEATURES
  CHECK_MUTEX
  ENABLE_ASSERT
  TRACE_MESSAGES
  ENABLE_EXTERNAL_CODE
)

set(_RELEASE_ENABLED_FEATURES
  ENABLE_ASSERT
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(_ENABLED_FEATURES ${_DEBUG_ENABLED_FEATURES})
else()
  set(_ENABLED_FEATURES ${_RELEASE_ENABLED_FEATURES})
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

foreach(var ${_ENABLED_FEATURES})
  list(FIND _FEATURES ${var} _idx)
  if(_idx EQUAL -1)
    message(FATAL_ERROR "Feature '${var}' is enabled but does not exist")
  endif()
  add_compile_definitions(PDP_${var})
endforeach()


add_subdirectory(core)
add_subdirectory(strings)
add_subdirectory(parser)

if(BUILD_TESTS)
  find_package(doctest REQUIRED)
  add_subdirectory(test)
endif()

add_executable(pdp main.cc gdb_session.cc
  $<TARGET_OBJECTS:pdp_core>
  $<TARGET_OBJECTS:pdp_strings>
  $<TARGET_OBJECTS:pdp_parser>
)
