cmake_minimum_required(VERSION 3.12)

project(pdp VERSION 1.0.0 DESCRIPTION "GDB/MI parsing and command processing library")

# Build unit tests

set(BUILD_TESTS ON)

# Build perf tests
set(BUILD_PERF ON)

# TODO: Future: investigate more compiler flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(BUILD_TESTS OFF)
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
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -fno-pie -no-pie -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

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
  # Trace RollingBuffer reallocation statistics 
  TRACE_ROLLING_BUFFER
  # Trace ChunkArray memory fragmentation 
  TRACE_CHUNK_ARRAY
  # Zero initialize values for better debugging
  ENABLE_ZERO_INITIALIZE
  # Trace search times in CallbackTable<>
  TRACE_CALLBACK_TABLE
  # Various convenience settings which should always run for debug builds
  DEBUG_BUILD
  # Trace RPC request/response tokens to debug mismatched or out-of-order Vim RPC calls
  TRACE_RPC_TOKENS
)

# TODO sanitizer and clang-tidy

# TODO Feature: ENABLE_BACKTRACE

# TODO: Experiment with LTO and add_library stuff

set(_DEBUG_ENABLED_FEATURES
  CHECK_MUTEX
  ENABLE_ASSERT
  TRACE_MESSAGES
  DEBUG_BUILD
  ENABLE_ZERO_INITIALIZE
  TRACE_RPC_TOKENS
)

set(_RELEASE_ENABLED_FEATURES
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
add_subdirectory(application)
add_subdirectory(handlers)

if(BUILD_TESTS)
  find_package(doctest REQUIRED)
  add_subdirectory(test)
endif()

if(BUILD_PERF)
  add_subdirectory(perf)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  find_path(BFD_INCLUDE_DIR NAMES bfd.h)
  find_library(BFD_LIBRARY NAMES bfd)
  add_executable(backtrace_filt backtrace_filt.cc)
  target_include_directories(backtrace_filt PRIVATE ${BFD_INCLUDE_DIR})
  target_link_libraries(backtrace_filt PRIVATE ${BFD_LIBRARY} pdp_strings)
endif()

add_executable(pdp main.cc)
target_link_libraries(pdp PRIVATE pdp_application pdp_handlers)
target_compile_features(pdp PRIVATE cxx_std_20)
