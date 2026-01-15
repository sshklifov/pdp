add_library(pdp_handlers STATIC
  debug_coordinator.cc
)

target_include_directories(pdp_handlers PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(pdp_handlers PRIVATE cxx_std_20)

target_link_libraries(pdp_handlers PUBLIC pdp_application)
