add_library(pdp_data INTERFACE)

target_include_directories(pdp_data INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pdp_data INTERFACE pdp_core pdp_tracing)
