add_library(pdp_core STATIC
    check.cc
    log.cc
)

# Depend on pdp_strings but don't list it! It's for a header-only class!
target_include_directories(pdp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
