add_library(pdp_core OBJECT
    check.cc
    log.cc
)

target_include_directories(pdp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
