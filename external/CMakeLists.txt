add_library(external INTERFACE)

target_include_directories(external INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(external INTERFACE pdp_core pdp_data)

