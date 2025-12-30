add_executable(test_vector test_vector.cc
  $<TARGET_OBJECTS:pdp_core>
)

add_executable(test_stack test_stack.cc
  $<TARGET_OBJECTS:pdp_core>
)

add_executable(test_core test_core.cc
  $<TARGET_OBJECTS:pdp_core>
)

target_compile_definitions(test_core PRIVATE
  PDP_TRACE_MESSAGES
  PDP_TRACE_BRANCH
)
