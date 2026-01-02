add_library(pdp_parser OBJECT
    parser.cc
    expr.cc
)

target_include_directories(pdp_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
