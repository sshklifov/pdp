add_library(pdp_strings OBJECT
  rolling_buffer.cc
  byte_stream.cc
)

target_include_directories(pdp_strings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
