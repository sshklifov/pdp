add_library(pdp_coroutine_handlers INTERFACE
  handle_breakpoint.cc
  handle_thread.cc
)

target_include_directories(pdp_coroutine_handlers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(pdp_coroutine_handlers INTERFACE cxx_std_20)
