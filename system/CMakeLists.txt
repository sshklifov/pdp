add_library(pdp_system STATIC
  file_descriptor.cc
)

target_include_directories(pdp_system PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pdp_system PUBLIC pdp_core)
