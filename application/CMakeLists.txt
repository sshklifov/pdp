add_library(pdp_application STATIC
  gdb_driver.cc
)

target_include_directories(pdp_application PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pdp_application PRIVATE
  pdp_core
  pdp_system
  pdp_tracing
  pdp_data
  pdp_strings
  pdp_parser
)
