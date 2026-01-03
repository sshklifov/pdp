add_library(pdp_tracing INTERFACE)

target_include_directories(pdp_tracing INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pdp_tracing INTERFACE pdp_core pdp_tracing)

