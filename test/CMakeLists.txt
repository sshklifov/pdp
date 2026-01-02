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

add_executable(test_string_slice test_string_slice.cc
  $<TARGET_OBJECTS:pdp_core>
  $<TARGET_OBJECTS:pdp_strings>
)

add_executable(test_string_builder test_string_builder.cc
  $<TARGET_OBJECTS:pdp_core>
  $<TARGET_OBJECTS:pdp_strings>
)

add_executable(test_rolling_buffer test_rolling_buffer.cc
  $<TARGET_OBJECTS:pdp_core>
  $<TARGET_OBJECTS:pdp_strings>
)

add_executable(test_parser test_parser.cc
  $<TARGET_OBJECTS:pdp_core>
  $<TARGET_OBJECTS:pdp_strings>
  $<TARGET_OBJECTS:pdp_parser>
)
