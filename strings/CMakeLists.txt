add_library(pdp_strings OBJECT
    string_builder.cc
    rolling_buffer.cc
)

target_include_directories(pdp_strings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
