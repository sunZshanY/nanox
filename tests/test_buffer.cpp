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

// ---------------------------------------------------------------------------
// Line-level editing (nano ^K, Vim dd / yy / o / O / D)
// ---------------------------------------------------------------------------

NX_TEST_CASE(delete_current_line_removes_it_and_returns_the_text) {
    auto b = TextBuffer::from_string("a\nb\nc");
    b.set_cursor(1, 0);
    NX_CHECK_EQ(b.delete_current_line(), std::string("b"));
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(2));
    NX_CHECK_EQ(b.lines()[0], std::string("a"));
    NX_CHECK_EQ(b.lines()[1], std::string("c"));
    NX_CHECK_EQ(b.row(), 1);
}

NX_TEST_CASE(delete_current_line_of_the_only_line_clears_it) {
    auto b = TextBuffer::from_string("only");
    NX_CHECK_EQ(b.delete_current_line(), std::string("only"));
    // The buffer always keeps at least one line.
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(1));
    NX_CHECK_EQ(b.lines()[0], std::string(""));
    NX_CHECK_EQ(b.col(), 0);
}

NX_TEST_CASE(delete_current_line_on_an_empty_buffer_is_a_no_op) {
    TextBuffer b;
    NX_CHECK_EQ(b.delete_current_line(), std::string(""));
    NX_CHECK(!b.dirty());
    NX_CHECK(!b.can_undo());  // nothing changed, so nothing to undo
}

NX_TEST_CASE(yank_line_does_not_modify_the_buffer) {
    auto b = TextBuffer::from_string("keep\nme");
    b.set_cursor(1, 0);
    NX_CHECK_EQ(b.yank_line(), std::string("me"));
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(2));
    NX_CHECK(!b.dirty());
}

NX_TEST_CASE(open_line_below_and_above_move_the_cursor) {
    auto b = TextBuffer::from_string("a\nb");
    b.set_cursor(0, 1);

    b.insert_line_below();
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(3));
    NX_CHECK_EQ(b.row(), 1);
    NX_CHECK_EQ(b.col(), 0);
    NX_CHECK_EQ(b.lines()[1], std::string(""));

    b.insert_line_above();
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(4));
    NX_CHECK_EQ(b.row(), 1);
}

NX_TEST_CASE(delete_to_end_of_line_keeps_the_prefix) {
    auto b = TextBuffer::from_string("hello world");
    b.set_cursor(0, 5);
    b.delete_to_end_of_line();
    NX_CHECK_EQ(b.lines()[0], std::string("hello"));
    NX_CHECK_EQ(b.col(), 5);
}

NX_TEST_CASE(delete_to_end_of_line_at_the_end_is_a_no_op) {
    auto b = TextBuffer::from_string("abc");
    b.set_cursor(0, 3);
    b.delete_to_end_of_line();
    NX_CHECK_EQ(b.lines()[0], std::string("abc"));
    NX_CHECK(!b.dirty());
}

// ---------------------------------------------------------------------------
// Undo / redo
// ---------------------------------------------------------------------------

NX_TEST_CASE(undo_restores_the_previous_text) {
    TextBuffer b;
    b.insert_text("hello");
    NX_CHECK(b.can_undo());
    b.undo();
    NX_CHECK_EQ(b.lines()[0], std::string(""));
    NX_CHECK(!b.can_undo());
    NX_CHECK(b.can_redo());
}

NX_TEST_CASE(a_typing_run_undoes_as_one_unit) {
    TextBuffer b;
    for (const char c : std::string("word")) {
        b.insert_char(c);
    }
    NX_CHECK_EQ(b.lines()[0], std::string("word"));
    b.undo();  // one step, not four
    NX_CHECK_EQ(b.lines()[0], std::string(""));
}

NX_TEST_CASE(a_cursor_move_splits_typing_into_separate_undo_units) {
    TextBuffer b;
    b.insert_char('a');
    b.set_cursor(0, 0);   // break the run
    b.insert_char('b');
    NX_CHECK_EQ(b.lines()[0], std::string("ba"));
    b.undo();
    NX_CHECK_EQ(b.lines()[0], std::string("a"));
    b.undo();
    NX_CHECK_EQ(b.lines()[0], std::string(""));
}

NX_TEST_CASE(redo_reapplies_an_undone_change) {
    TextBuffer b;
    b.insert_text("abc");
    b.undo();
    NX_CHECK_EQ(b.lines()[0], std::string(""));
    b.redo();
    NX_CHECK_EQ(b.lines()[0], std::string("abc"));
    NX_CHECK(!b.can_redo());
}

NX_TEST_CASE(a_new_edit_discards_the_redo_stack) {
    TextBuffer b;
    b.insert_text("first");
    b.undo();
    NX_CHECK(b.can_redo());
    b.insert_text("second");
    NX_CHECK(!b.can_redo());
    NX_CHECK_EQ(b.lines()[0], std::string("second"));
}

NX_TEST_CASE(undo_and_redo_at_the_ends_are_no_ops) {
    TextBuffer b;
    b.undo();
    b.redo();
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(1));
    NX_CHECK_EQ(b.lines()[0], std::string(""));
}

NX_TEST_CASE(undo_is_available_for_every_kind_of_edit) {
    auto b = TextBuffer::from_string("one\ntwo");
    b.set_cursor(0, 0);
    b.delete_current_line();
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(1));
    b.undo();
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(2));
    NX_CHECK_EQ(b.lines()[0], std::string("one"));

    b.insert_line_below();
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(3));
    b.undo();
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(2));

    b.set_cursor(0, 1);
    b.delete_to_end_of_line();
    NX_CHECK_EQ(b.lines()[0], std::string("o"));
    b.undo();
    NX_CHECK_EQ(b.lines()[0], std::string("one"));
}

NX_TEST_CASE(dirty_clears_when_undo_returns_to_the_saved_state) {
    TextBuffer b;
    b.insert_text("typed");
    NX_CHECK(b.dirty());
    b.mark_clean();
    NX_CHECK(!b.dirty());
    b.insert_text("more");
    NX_CHECK(b.dirty());
    b.undo();
    // Back at the state that was saved, so it is genuinely unmodified again.
    NX_CHECK(!b.dirty());
}

NX_TEST_CASE(linewise_paste_inserts_a_whole_line) {
    auto b = TextBuffer::from_string("a\nb");
    b.set_cursor(0, 0);
    b.insert_line_at(1, "pasted");
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(3));
    NX_CHECK_EQ(b.lines()[1], std::string("pasted"));
    b.undo();
    NX_CHECK_EQ(b.line_count(), static_cast<std::size_t>(2));
}

// ---------------------------------------------------------------------------
// Search / replace
// ---------------------------------------------------------------------------

NX_TEST_CASE(find_forward_moves_the_cursor_to_the_match) {
    auto b = TextBuffer::from_string("alpha\nbeta\ngamma");
    NX_CHECK(b.find_forward("beta"));
    NX_CHECK_EQ(b.row(), 1);
    NX_CHECK_EQ(b.col(), 0);
}

NX_TEST_CASE(find_forward_wraps_around_the_end) {
    auto b = TextBuffer::from_string("needle\nhay");
    b.set_cursor(1, 0);
    NX_CHECK(b.find_forward("needle"));
    NX_CHECK_EQ(b.row(), 0);
}

NX_TEST_CASE(find_forward_reports_a_miss_and_keeps_the_cursor) {
    auto b = TextBuffer::from_string("abc");
    b.set_cursor(0, 2);
    NX_CHECK(!b.find_forward("zzz"));
    NX_CHECK_EQ(b.row(), 0);
    NX_CHECK_EQ(b.col(), 2);
}

NX_TEST_CASE(find_forward_rejects_an_empty_or_multiline_needle) {
    auto b = TextBuffer::from_string("abc");
    NX_CHECK(!b.find_forward(""));
    NX_CHECK(!b.find_forward("a\nb"));
}

NX_TEST_CASE(replace_all_replaces_every_occurrence_from_the_cursor) {
    auto b = TextBuffer::from_string("a cat and a cat");
    NX_CHECK_EQ(b.replace_all_forward("cat", "dog"), static_cast<std::size_t>(2));
    NX_CHECK_EQ(b.lines()[0], std::string("a dog and a dog"));
    NX_CHECK_EQ(b.row(), 0);
    NX_CHECK_EQ(b.col(), 2);  // cursor lands on the first replacement
}

NX_TEST_CASE(replace_all_is_one_undo_step) {
    auto b = TextBuffer::from_string("x x x");
    NX_CHECK_EQ(b.replace_all_forward("x", "y"), static_cast<std::size_t>(3));
    b.undo();
    NX_CHECK_EQ(b.lines()[0], std::string("x x x"));
}

NX_TEST_CASE(replace_all_does_not_loop_when_the_replacement_contains_the_needle) {
    auto b = TextBuffer::from_string("a");
    NX_CHECK_EQ(b.replace_all_forward("a", "aa"), static_cast<std::size_t>(1));
    NX_CHECK_EQ(b.lines()[0], std::string("aa"));
}

NX_TEST_CASE(replace_all_with_a_missing_needle_changes_nothing) {
    auto b = TextBuffer::from_string("abc");
    NX_CHECK_EQ(b.replace_all_forward("zzz", "y"), static_cast<std::size_t>(0));
    NX_CHECK(!b.dirty());
    NX_CHECK(!b.can_undo());
}
