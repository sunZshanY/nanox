#include "nanox/editor/DisplayWidth.h"

#include "test_framework.hpp"

#include <string>

using namespace nanox::editor;

NX_TEST_CASE(ascii_is_one_cell_each) {
    NX_CHECK_EQ(display_width(""), std::size_t{0});
    NX_CHECK_EQ(display_width("abc"), std::size_t{3});
    NX_CHECK_EQ(display_width("NanoX 0.1.0"), std::size_t{11});
}

NX_TEST_CASE(tui_glyphs_stay_narrow) {
    // The box-drawing and symbol glyphs the TUI itself emits must stay 1 cell,
    // otherwise every frame would misalign.
    NX_CHECK_EQ(display_width("│"), std::size_t{1});
    NX_CHECK_EQ(display_width("─"), std::size_t{1});
    NX_CHECK_EQ(display_width("▾▸●"), std::size_t{3});
    NX_CHECK_EQ(display_width("✓✗…—"), std::size_t{4});
}

NX_TEST_CASE(cjk_glyphs_are_double_width) {
    NX_CHECK_EQ(display_width("桌面"), std::size_t{4});
    NX_CHECK_EQ(display_width("中文abc"), std::size_t{7});
    NX_CHECK_EQ(display_width("日本語"), std::size_t{6});
    NX_CHECK_EQ(display_width("한글"), std::size_t{4});
    NX_CHECK_EQ(display_width("ＡＢ"), std::size_t{4});  // fullwidth latin
}

NX_TEST_CASE(combining_marks_are_zero_width) {
    // "e" + U+0301 combining acute accent renders as one cell.
    NX_CHECK_EQ(display_width("e\xCC\x81"), std::size_t{1});
    // Zero-width joiner / non-joiner.
    NX_CHECK_EQ(display_width("\xE2\x80\x8D"), std::size_t{0});
}

NX_TEST_CASE(invalid_utf8_does_not_underflow) {
    NX_CHECK_EQ(display_width("\xFF"), std::size_t{1});
    NX_CHECK_EQ(display_width("\xE4\xB8"), std::size_t{2});  // truncated CJK
}

NX_TEST_CASE(truncate_respects_cell_width) {
    NX_CHECK_EQ(truncate_to_width("abcdef", 3), std::string("abc"));
    NX_CHECK_EQ(truncate_to_width("abc", 10), std::string("abc"));
}

NX_TEST_CASE(truncate_never_splits_a_wide_glyph) {
    // Two CJK glyphs are 4 cells; only the first fits in 3 cells.
    NX_CHECK_EQ(truncate_to_width("桌面", 3), std::string("桌"));
    NX_CHECK_EQ(truncate_to_width("桌面", 4), std::string("桌面"));
    NX_CHECK_EQ(truncate_to_width("abc桌", 4), std::string("abc"));
}

NX_TEST_CASE(truncate_keeps_zero_width_marks_with_their_base) {
    // The combining mark adds no cells, so it stays attached to "e".
    NX_CHECK_EQ(truncate_to_width("e\xCC\x81xyz", 2), std::string("e\xCC\x81x"));
}
