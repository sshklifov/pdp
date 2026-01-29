add_library(pdp_drivers STATIC
  gdb_driver.cc
  vim_driver.cc
  ssh_driver.cc
  breakpoint_table.cc
)

target_include_directories(pdp_drivers PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pdp_drivers PUBLIC
  pdp_core
  pdp_system
  pdp_tracing
  pdp_data
  pdp_strings
  pdp_parser
)
