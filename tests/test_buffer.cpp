#include "nanox/editor/TextBuffer.h"

#include "test_framework.hpp"

#include <cstddef>
#include <string>

using namespace nanox::editor;

NX_TEST_CASE(empty_buffer_has_one_empty_line) {
    TextBuffer b;
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(1));
    NX_CHECK_EQ(b.lines()[0], std::string(""));
    NX_CHECK_EQ(b.row(), 0);
    NX_CHECK_EQ(b.col(), 0);
    NX_CHECK(!b.dirty());
}

NX_TEST_CASE(from_string_splits_lines_and_drops_trailing_newline) {
    auto b = TextBuffer::from_string("abc\ndef\n");
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(2));
    NX_CHECK_EQ(b.lines()[0], std::string("abc"));
    NX_CHECK_EQ(b.lines()[1], std::string("def"));
}

NX_TEST_CASE(from_string_keeps_inner_blank_lines) {
    auto b = TextBuffer::from_string("a\n\nb");
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(3));
    NX_CHECK_EQ(b.lines()[0], std::string("a"));
    NX_CHECK_EQ(b.lines()[1], std::string(""));
    NX_CHECK_EQ(b.lines()[2], std::string("b"));
}

NX_TEST_CASE(to_string_round_trips) {
    auto b = TextBuffer::from_string("let x: int = 10;\n\nprintln(x);\n");
    NX_CHECK_EQ(b.to_string(), std::string("let x: int = 10;\n\nprintln(x);\n"));
}

NX_TEST_CASE(crlf_input_is_normalized) {
    auto b = TextBuffer::from_string("a\r\nb\r\n");
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(2));
    NX_CHECK_EQ(b.lines()[0], std::string("a"));
    NX_CHECK_EQ(b.lines()[1], std::string("b"));
}

NX_TEST_CASE(tabs_are_expanded_on_load) {
    auto b = TextBuffer::from_string("fn\tmain");
    NX_CHECK_EQ(b.lines()[0], std::string("fn    main"));
}

NX_TEST_CASE(insert_char_moves_cursor_and_marks_dirty) {
    TextBuffer b;
    b.insert_char('f');
    b.insert_char('n');
    NX_CHECK_EQ(b.lines()[0], std::string("fn"));
    NX_CHECK_EQ(b.col(), 2);
    NX_CHECK(b.dirty());
}

NX_TEST_CASE(insert_text_advances_cursor_by_text_length) {
    TextBuffer b;
    b.insert_text("let");
    NX_CHECK_EQ(b.lines()[0], std::string("let"));
    NX_CHECK_EQ(b.col(), 3);
}

NX_TEST_CASE(insert_newline_splits_the_line) {
    TextBuffer b;
    b.insert_text("ab");
    b.insert_newline();
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(2));
    NX_CHECK_EQ(b.lines()[0], std::string("ab"));
    NX_CHECK_EQ(b.lines()[1], std::string(""));
    NX_CHECK_EQ(b.row(), 1);
    NX_CHECK_EQ(b.col(), 0);
}

NX_TEST_CASE(backspace_mid_line) {
    TextBuffer b;
    b.insert_text("abc");
    b.backspace();
    NX_CHECK_EQ(b.lines()[0], std::string("ab"));
    NX_CHECK_EQ(b.col(), 2);
}

NX_TEST_CASE(backspace_at_line_start_joins_with_previous_line) {
    auto b = TextBuffer::from_string("hello\nworld");
    b.set_cursor(1, 0);
    b.backspace();
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(1));
    NX_CHECK_EQ(b.lines()[0], std::string("helloworld"));
    NX_CHECK_EQ(b.row(), 0);
    NX_CHECK_EQ(b.col(), 5);
}

NX_TEST_CASE(delete_char_at_line_end_joins_next_line) {
    auto b = TextBuffer::from_string("ab\ncd");
    b.set_cursor(0, 2);
    b.delete_char();
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(1));
    NX_CHECK_EQ(b.lines()[0], std::string("abcd"));
    NX_CHECK_EQ(b.col(), 2);
}

NX_TEST_CASE(cursor_is_clamped_when_moving_to_shorter_line) {
    auto b = TextBuffer::from_string("longline\nx");
    b.set_cursor(0, 8);
    b.move_down();
    NX_CHECK_EQ(b.row(), 1);
    NX_CHECK_EQ(b.col(), 1);  // clamped to the length of "x"
    b.move_up();
    NX_CHECK_EQ(b.row(), 0);
    NX_CHECK_EQ(b.col(), 1);  // stays clamped
}

NX_TEST_CASE(move_left_at_line_start_goes_to_previous_line_end) {
    auto b = TextBuffer::from_string("ab\ncd");
    b.set_cursor(1, 0);
    b.move_left();
    NX_CHECK_EQ(b.row(), 0);
    NX_CHECK_EQ(b.col(), 2);
}

NX_TEST_CASE(move_right_at_line_end_goes_to_next_line_start) {
    auto b = TextBuffer::from_string("ab\ncd");
    b.set_cursor(0, 2);
    b.move_right();
    NX_CHECK_EQ(b.row(), 1);
    NX_CHECK_EQ(b.col(), 0);
}

NX_TEST_CASE(home_and_end) {
    auto b = TextBuffer::from_string("abcd");
    b.set_cursor(0, 2);
    b.move_home();
    NX_CHECK_EQ(b.col(), 0);
    b.move_end();
    NX_CHECK_EQ(b.col(), 4);
}

NX_TEST_CASE(set_cursor_clamps_out_of_range_values) {
    auto b = TextBuffer::from_string("ab");
    b.set_cursor(99, 99);
    NX_CHECK_EQ(b.row(), 0);
    NX_CHECK_EQ(b.col(), 2);
    b.set_cursor(-5, -5);
    NX_CHECK_EQ(b.row(), 0);
    NX_CHECK_EQ(b.col(), 0);
}

NX_TEST_CASE(mark_clean_resets_dirty) {
    TextBuffer b;
    b.insert_char('x');
    NX_CHECK(b.dirty());
    b.mark_clean();
    NX_CHECK(!b.dirty());
}

NX_TEST_CASE(page_moves_clamp_to_bounds) {
    auto b = TextBuffer::from_string("a\nb\nc\nd");
    b.move_page_down(2);
    NX_CHECK_EQ(b.row(), 2);
    b.move_page_down(99);
    NX_CHECK_EQ(b.row(), 3);
    b.move_page_up(99);
    NX_CHECK_EQ(b.row(), 0);
}

NX_TEST_CASE(edits_at_buffer_start_are_no_ops) {
    TextBuffer b;
    b.backspace();
    b.delete_char();
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(1));
    NX_CHECK_EQ(b.lines()[0], std::string(""));
    NX_CHECK(!b.dirty());
}
