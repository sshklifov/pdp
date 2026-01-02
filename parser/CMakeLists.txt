add_library(pdp_parser OBJECT
    mi_parser.cc
    mi_expr.cc
    rpc_parser.cc
)

target_include_directories(pdp_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
