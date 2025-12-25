cmake_minimum_required(VERSION 3.12)

project(pdp VERSION 1.0.0 DESCRIPTION "GDB/MI parsing and command processing library")

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -O0 -ggdb")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g -O2 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -Wall")

option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
  find_package(doctest REQUIRED)
  add_executable(test test.cc)
endif()

add_executable(pdp main.cc log.cc)
