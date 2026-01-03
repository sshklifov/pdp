add_library(pdp_parser OBJECT
    expr.cc
    mi_parser.cc
    rpc_parser.cc
)

target_include_directories(pdp_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
