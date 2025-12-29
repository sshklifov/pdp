add_library(pdp_data OBJECT
    allocator.cc
)

target_include_directories(pdp_data PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
