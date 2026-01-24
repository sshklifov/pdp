add_library(pdp_tracing STATIC
  execution_tracer.cc
)

target_compile_definitions(pdp_tracing PRIVATE
  PDP_RECORDER_PATH=\"${CMAKE_BINARY_DIR}/capture.bin\"
)

target_include_directories(pdp_tracing PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pdp_tracing PUBLIC pdp_core)
