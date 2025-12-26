cmake_minimum_required(VERSION 3.12)

project(pdp VERSION 1.0.0 DESCRIPTION "GDB/MI parsing and command processing library")

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -O0 -ggdb")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g -O2 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -Wall")

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
  # Report branch mispredictions originating from PDP_LIKELY 
  TRACE_BRANCH
  # Allow use of external, unverified code 
  ENABLE_EXTERNAL_CODE
  # Trace RollingBuffer reallocation statistics 
  TRACE_ROLLING_BUFFER
)


set(_DEBUG_ENABLED_FEATURES
  CHECK_MUTEX
  ENABLE_ASSERT
  TRACE_MESSAGES
  ENABLE_EXTERNAL_CODE
)

set(_RELEASE_ENABLED_FEATURES
  ENABLE_ASSERT
)

add_library(pdp_core
  check.cc
  log.cc
  rolling_buffer.cc
  string_builder.cc
  string_slice.cc
  allocator.cc
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(_ENABLED_FEATURES ${_DEBUG_ENABLED_FEATURES})
else()
  set(_ENABLED_FEATURES ${_RELEASE_ENABLED_FEATURES})
endif()

foreach(var ${_ENABLED_FEATURES})
  list(FIND _FEATURES ${var} _idx)
  if(_idx EQUAL -1)
    message(FATAL_ERROR "Feature '${var}' is enabled but does not exist")
  endif()
  target_compile_definitions(pdp_core PUBLIC PDP_${var})
endforeach()


target_include_directories(pdp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pdp_core PUBLIC pthread)

if(BUILD_TESTS)
  find_package(doctest REQUIRED)
  add_executable(test test.cc)
  target_link_libraries(test PRIVATE pdp_core)
endif()

add_executable(pdp main.cc gdb_session.cc)
target_link_libraries(pdp PRIVATE pdp_core)
