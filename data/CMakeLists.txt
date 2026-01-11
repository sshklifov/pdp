add_library(pdp_data STATIC
  chunk_array.cc
)

target_include_directories(pdp_data PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pdp_data PUBLIC pdp_core pdp_tracing)
