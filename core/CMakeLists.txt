add_library(pdp_core STATIC
    check.cc
    log.cc
    backtrace.cc
)

# Depend on pdp_strings but don't list it! It's for a header-only class!
target_include_directories(pdp_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
