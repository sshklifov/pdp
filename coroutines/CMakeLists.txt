add_subdirectory(handlers)

add_library(pdp_coroutines STATIC
  gdb_async_driver.cc
  vim_async_driver.cc
  debug_coordinator.cc
)

target_include_directories(pdp_coroutines PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(pdp_coroutines PRIVATE cxx_std_20)

target_link_libraries(pdp_coroutines
  PUBLIC pdp_drivers
  PRIVATE pdp_coroutine_handlers
)
