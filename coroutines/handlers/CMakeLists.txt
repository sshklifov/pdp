add_library(pdp_coroutine_handlers
  handle_breakpoint.cc
  handle_thread.cc
)

target_include_directories(pdp_coroutine_handlers PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(pdp_coroutine_handlers PRIVATE cxx_std_20)
