add_library(pdp_strings STATIC
  rolling_buffer.cc
  byte_stream.cc
)

target_include_directories(pdp_strings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pdp_strings PUBLIC pdp_core pdp_data pdp_system pdp_tracing)
