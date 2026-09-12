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

# Editor tests exercise the pure logic of the editor library (TextBuffer,
# Keymap, CommandParser) without needing a terminal.
function(nanox_add_editor_test name)
    add_executable(${name} ${name}.cpp test_main.cpp)
    target_link_libraries(${name} PRIVATE nanox::editor)
    nanox_set_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

nanox_add_editor_test(test_buffer)
nanox_add_editor_test(test_keymap)
nanox_add_editor_test(test_command_parser)
nanox_add_editor_test(test_display_width)
