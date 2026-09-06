add_executable(lexer_dump lexer_dump.cpp)
target_link_libraries(lexer_dump PRIVATE nanox::core)

add_executable(nanox nanox.cpp)
target_link_libraries(nanox PRIVATE nanox::editor)
