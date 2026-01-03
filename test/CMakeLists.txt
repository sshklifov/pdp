add_executable(test_vector test_vector.cc)
target_link_libraries(test_vector PRIVATE pdp_data)

add_executable(test_stack test_stack.cc)
target_link_libraries(test_stack PRIVATE pdp_data)


add_executable(test_core test_core.cc)
target_link_libraries(test_core PRIVATE pdp_core)

add_executable(test_string_slice test_string_slice.cc)
target_link_libraries(test_string_slice PRIVATE pdp_strings)

add_executable(test_string_builder test_string_builder.cc)
target_link_libraries(test_string_builder PRIVATE pdp_strings)

add_executable(test_rolling_buffer test_rolling_buffer.cc)
target_link_libraries(test_rolling_buffer PRIVATE pdp_strings)

add_executable(test_mi_parser test_mi_parser.cc)
target_link_libraries(test_mi_parser PRIVATE pdp_parser)

add_executable(test_arena test_arena.cc)
target_link_libraries(test_arena PRIVATE pdp_data)

add_executable(test_chunk_array test_chunk_array.cc)
target_link_libraries(test_chunk_array PRIVATE pdp_data)

add_executable(test_file_descriptors test_file_descriptors.cc)
target_link_libraries(test_file_descriptors PRIVATE pdp_system)

add_executable(test_byte_stream test_byte_stream.cc)
target_link_libraries(test_byte_stream PRIVATE pdp_strings)

add_executable(test_rpc_parser test_rpc_parser.cc)
target_link_libraries(test_rpc_parser PRIVATE pdp_parser)
