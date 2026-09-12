#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace nanox::editor {

// Number of terminal cells `text` occupies, using a compact East Asian Width
// table:
//   - combining / zero-width marks (U+0300..., ZWJ, variation selectors): 0
//   - CJK, Hangul, Kana and fullwidth forms:                              2
//   - everything else (including the TUI's own box-drawing glyphs):       1
//
// Invalid UTF-8 bytes count as 1 so a corrupt byte can never make a row look
// narrower than it really is (every framed row must fill exactly `cols` cells).
std::size_t display_width(std::string_view text);

// Longest prefix of `text` that fits in at most `width` cells. Never splits a
// UTF-8 sequence and never emits half of a wide glyph: a 2-cell character that
// does not fit in the remaining space is dropped entirely.
std::string truncate_to_width(std::string_view text, std::size_t width);

}  // namespace nanox::editor
