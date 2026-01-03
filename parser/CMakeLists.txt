add_library(pdp_parser STATIC
    expr.cc
    mi_parser.cc
    rpc_parser.cc
)

target_include_directories(pdp_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pdp_parser PUBLIC pdp_core pdp_strings pdp_data pdp_tracing external)
