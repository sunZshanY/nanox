# One test executable per source file. Each test file registers its cases via
# the NX_TEST_CASE macro and links the tiny shared runner in test_main.cpp.
function(nanox_add_test name)
    add_executable(${name} ${name}.cpp test_main.cpp)
    target_link_libraries(${name} PRIVATE nanox::core)
    nanox_set_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

nanox_add_test(test_token)
nanox_add_test(test_diagnostic)
nanox_add_test(test_lexer)
nanox_add_test(test_filetree)
nanox_add_test(test_project)

# Editor tests exercise TextBuffer (pure logic) and link the editor library.
add_executable(test_buffer test_buffer.cpp test_main.cpp)
target_link_libraries(test_buffer PRIVATE nanox::editor)
nanox_set_warnings(test_buffer)
add_test(NAME test_buffer COMMAND test_buffer)
