cmake_minimum_required(VERSION 3.12)

project(pdp VERSION 1.0.0 DESCRIPTION "GDB/MI parsing and command processing library")

# Build unit tests
set(BUILD_TESTS OFF)

# Build perf tests
set(BUILD_PERF ON)

# TODO: Future: investigate more compiler flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  add_compile_options(-Wall -Wno-unused-variable
    -fno-exceptions -fno-unwind-tables -fasynchronous-unwind-tables
    -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit
    -nostdlib++
  )
else()
  set(BUILD_PERF OFF)
  add_compile_options(-Wall -Wextra)
endif()

set(CMAKE_CXX_STANDARD 17)

# TODO: Future: Remove debugging information
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -fno-pie -no-pie")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g1 -fno-omit-frame-pointer -O3")

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
  # Trace ChunkArray memory fragmentation 
  TRACE_CHUNK_ARRAY
  # Zero initialize values for better debugging
  ENABLE_ZERO_INITIALIZE
  # Trace search times in CallbackTable<>
  TRACE_CALLBACK_TABLE
)

# TODO sanitizer and clang-tidy

# TODO Feature: ENABLE_BACKTRACE

# TODO: Experiment with LTO and add_library stuff

set(_DEBUG_ENABLED_FEATURES
  CHECK_MUTEX
  ENABLE_ASSERT
  TRACE_MESSAGES
  ENABLE_EXTERNAL_CODE
  ENABLE_ZERO_INITIALIZE
  TRACE_CALLBACK_TABLE
)

set(_RELEASE_ENABLED_FEATURES
  ENABLE_EXTERNAL_CODE
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
add_subdirectory(system)
add_subdirectory(tracing)
add_subdirectory(data)
add_subdirectory(strings)
add_subdirectory(external)
add_subdirectory(parser)

if(BUILD_TESTS)
  find_package(doctest REQUIRED)
  add_subdirectory(test)
endif()

if(BUILD_PERF)
  add_subdirectory(perf)
endif()

add_executable(pdp main.cc gdb_session.cc)
target_link_libraries(pdp PRIVATE
  pdp_core
  pdp_system
  pdp_tracing
  pdp_data
  pdp_strings
  pdp_parser
)
