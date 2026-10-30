function(json99_add_test name)
  add_executable(${name} ${name}.c)
  json_target_defaults(${name})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

json99_add_test(test_json)
target_link_libraries(test_json PRIVATE json99)

json99_add_test(test_raw_string)
target_link_libraries(test_raw_string PRIVATE json99)

json99_add_test(test_unicode_writer)
target_link_libraries(test_unicode_writer PRIVATE json99)

json99_add_test(test_number_writer)
target_link_libraries(test_number_writer PRIVATE json99)

if(TARGET sio99_stdio)
  json99_add_test(test_file_reader)
  target_link_libraries(test_file_reader PRIVATE json99 sio99_stdio)
endif()

if(JSON99_WITH_HEAP)
  json99_add_test(test_heap)
  target_link_libraries(test_heap PRIVATE json99 json99_heap)
  json99_add_test(test_dynamic_buffer_writer)
  target_link_libraries(test_dynamic_buffer_writer PRIVATE json99 json99_heap)
endif()

json99_add_test(test_io_errors)
target_link_libraries(test_io_errors PRIVATE json99)
