#include "nanox/editor/DisplayWidth.h"

namespace nanox::editor {

namespace {

// True for code points that occupy no cells on their own: combining marks,
// zero-width spaces/joiners, variation selectors and the like.
bool is_zero_width(char32_t cp) {
    if (cp == 0x0000) {
        return true;
    }
    // Combining Diacritical Marks and the common script-specific mark blocks.
    if (cp >= 0x0300 && cp <= 0x036F) return true;
    if (cp >= 0x0483 && cp <= 0x0489) return true;
    if (cp >= 0x0591 && cp <= 0x05BD) return true;
    if (cp == 0x05BF || cp == 0x05C1 || cp == 0x05C2 || cp == 0x05C4 || cp == 0x05C5 ||
        cp == 0x05C7) {
        return true;
    }
    if (cp >= 0x0610 && cp <= 0x061A) return true;
    if (cp >= 0x064B && cp <= 0x065F) return true;
    if (cp >= 0x0670 && cp <= 0x0670) return true;
    if (cp >= 0x06D6 && cp <= 0x06DC) return true;
    if (cp >= 0x06DF && cp <= 0x06E4) return true;
    if (cp >= 0x06E7 && cp <= 0x06E8) return true;
    if (cp >= 0x06EA && cp <= 0x06ED) return true;
    if (cp == 0x0711) return true;
    if (cp >= 0x0730 && cp <= 0x074A) return true;
    if (cp >= 0x07A6 && cp <= 0x07B0) return true;
    if (cp >= 0x07EB && cp <= 0x07F3) return true;
    if (cp >= 0x0816 && cp <= 0x0819) return true;
    if (cp >= 0x081B && cp <= 0x0823) return true;
    if (cp >= 0x0825 && cp <= 0x0827) return true;
    if (cp >= 0x0829 && cp <= 0x082D) return true;
    if (cp >= 0x0859 && cp <= 0x085B) return true;
    if (cp >= 0x08D3 && cp <= 0x08E1) return true;
    if (cp >= 0x08E3 && cp <= 0x0902) return true;
    if (cp == 0x093A) return true;
    if (cp == 0x093C) return true;
    if (cp >= 0x0941 && cp <= 0x0948) return true;
    if (cp == 0x094D) return true;
    if (cp >= 0x0951 && cp <= 0x0957) return true;
    if (cp >= 0x0962 && cp <= 0x0963) return true;
    if (cp >= 0x0981 && cp <= 0x0981) return true;
    if (cp == 0x09BC) return true;
    if (cp >= 0x09C1 && cp <= 0x09C4) return true;
    if (cp == 0x09CD) return true;
    if (cp >= 0x09E2 && cp <= 0x09E3) return true;
    if (cp >= 0x0A01 && cp <= 0x0A02) return true;
    if (cp == 0x0A3C) return true;
    if (cp >= 0x0A41 && cp <= 0x0A42) return true;
    if (cp >= 0x0A47 && cp <= 0x0A48) return true;
    if (cp >= 0x0A4B && cp <= 0x0A4D) return true;
    if (cp == 0x0A51) return true;
    if (cp >= 0x0A70 && cp <= 0x0A71) return true;
    if (cp == 0x0A75) return true;
    if (cp >= 0x0A81 && cp <= 0x0A82) return true;
    if (cp == 0x0ABC) return true;
    if (cp >= 0x0AC1 && cp <= 0x0AC5) return true;
    if (cp >= 0x0AC7 && cp <= 0x0AC8) return true;
    if (cp == 0x0ACD) return true;
    if (cp >= 0x0AE2 && cp <= 0x0AE3) return true;
    if (cp >= 0x0AFA && cp <= 0x0AFF) return true;
    if (cp == 0x0B01) return true;
    if (cp == 0x0B3C) return true;
    if (cp == 0x0B3F) return true;
    if (cp >= 0x0B41 && cp <= 0x0B44) return true;
    if (cp == 0x0B4D) return true;
    if (cp >= 0x0B56 && cp <= 0x0B56) return true;
    if (cp >= 0x0B62 && cp <= 0x0B63) return true;
    if (cp == 0x0B82) return true;
    if (cp == 0x0BC0) return true;
    if (cp == 0x0BCD) return true;
    if (cp >= 0x0BF3 && cp <= 0x0BFA) return true;
    if (cp == 0x0C00 || cp == 0x0C04 || cp == 0x0C3E || cp == 0x0C3F ||
        cp == 0x0C40 || cp == 0x0C46 || cp == 0x0C47 || cp == 0x0C48 ||
        cp == 0x0C4A || cp == 0x0C4B || cp == 0x0C4C || cp == 0x0C4D ||
        cp == 0x0C55 || cp == 0x0C56 || cp == 0x0C81 || cp == 0x0CBC ||
        cp == 0x0CBF || cp == 0x0CCC || cp == 0x0CCD || cp == 0x0CE2 ||
        cp == 0x0CE3 || cp == 0x0D00 || cp == 0x0D01 || cp == 0x0D3B ||
        cp == 0x0D3C || cp == 0x0D41 || cp == 0x0D42 || cp == 0x0D43 ||
        cp == 0x0D44 || cp == 0x0D4D || cp == 0x0D62 || cp == 0x0D63 ||
        cp == 0x0DCA || cp == 0x0DD2 || cp == 0x0DD3 || cp == 0x0DD4 ||
        cp == 0x0DD6 || cp == 0x0E31 || cp == 0x0E34 || cp == 0x0E35 ||
        cp == 0x0E36 || cp == 0x0E37 || cp == 0x0E38 || cp == 0x0E39 ||
        cp == 0x0E3A) {
        return true;
    }
    if (cp >= 0x0E47 && cp <= 0x0E4E) return true;
    if (cp == 0x0EB1) return true;
    if (cp >= 0x0EB4 && cp <= 0x0EB9) return true;
    if (cp >= 0x0EBB && cp <= 0x0EBC) return true;
    if (cp >= 0x0EC8 && cp <= 0x0ECD) return true;
    if (cp >= 0x0F18 && cp <= 0x0F19) return true;
    if (cp == 0x0F35 || cp == 0x0F37 || cp == 0x0F39) return true;
    if (cp >= 0x0F71 && cp <= 0x0F7E) return true;
    if (cp >= 0x0F80 && cp <= 0x0F84) return true;
    if (cp >= 0x0F86 && cp <= 0x0F87) return true;
    if (cp >= 0x0F8D && cp <= 0x0F97) return true;
    if (cp >= 0x0F99 && cp <= 0x0FBC) return true;
    if (cp == 0x0FC6) return true;
    // Zero-width spaces, joiners, marks and directional controls.
    if (cp == 0x200B || cp == 0x200C || cp == 0x200D || cp == 0x200E || cp == 0x200F) {
        return true;
    }
    if (cp >= 0x202A && cp <= 0x202E) return true;
    if (cp >= 0x2060 && cp <= 0x2064) return true;
    if (cp >= 0x2066 && cp <= 0x206F) return true;
    if (cp == 0xFEFF) return true;
    // Combining marks and variation selectors.
    if (cp >= 0x20D0 && cp <= 0x20F0) return true;
    if (cp >= 0xFE00 && cp <= 0xFE0F) return true;
    if (cp >= 0xFE20 && cp <= 0xFE2F) return true;
    if (cp >= 0x1AB0 && cp <= 0x1AFF) return true;
    if (cp >= 0x1DC0 && cp <= 0x1DFF) return true;
    if (cp >= 0xE0100 && cp <= 0xE01EF) return true;
    return false;
}

// True for code points that occupy two terminal cells (East Asian Wide or
// Fullwidth). The ranges cover CJK, Hangul, Kana, fullwidth forms and the
// emoji blocks, which is what a source-file TUI realistically sees.
bool is_wide(char32_t cp) {
    if (cp >= 0x1100 && cp <= 0x115F) return true;  // Hangul Jamo
    if (cp >= 0x2E80 && cp <= 0x303E) return true;  // CJK radicals, Kangxi, symbols
    if (cp >= 0x3041 && cp <= 0x33FF) return true;  // Kana, Bopomofo, compat
    if (cp >= 0x3400 && cp <= 0x4DBF) return true;  // CJK Ext A
    if (cp >= 0x4E00 && cp <= 0x9FFF) return true;  // CJK Unified
    if (cp >= 0xA000 && cp <= 0xA4CF) return true;  // Yi
    if (cp >= 0xA960 && cp <= 0xA97F) return true;  // Hangul Jamo Extended-A
    if (cp >= 0xAC00 && cp <= 0xD7A3) return true;  // Hangul syllables
    if (cp >= 0xF900 && cp <= 0xFAFF) return true;  // CJK compat ideographs
    if (cp >= 0xFE10 && cp <= 0xFE19) return true;  // vertical forms
    if (cp >= 0xFE30 && cp <= 0xFE6F) return true;  // CJK compat / small forms
    if (cp >= 0xFF00 && cp <= 0xFF60) return true;  // fullwidth forms
    if (cp >= 0xFFE0 && cp <= 0xFFE6) return true;  // fullwidth signs
    if (cp >= 0x16FE0 && cp <= 0x16FE4) return true;
    if (cp >= 0x17000 && cp <= 0x18AFF) return true;  // Tangut
    if (cp >= 0x1B000 && cp <= 0x1B2FF) return true;  // Kana supplement
    if (cp >= 0x1F004 && cp <= 0x1F004) return true;
    if (cp >= 0x1F0CF && cp <= 0x1F0CF) return true;
    if (cp >= 0x1F18E && cp <= 0x1F18E) return true;
    if (cp >= 0x1F191 && cp <= 0x1F19A) return true;
    if (cp >= 0x1F200 && cp <= 0x1F320) return true;
    if (cp >= 0x1F32D && cp <= 0x1F335) return true;
    if (cp >= 0x1F337 && cp <= 0x1F37C) return true;
    if (cp >= 0x1F37E && cp <= 0x1F393) return true;
    if (cp >= 0x1F3A0 && cp <= 0x1F3CA) return true;
    if (cp >= 0x1F3CF && cp <= 0x1F3D3) return true;
    if (cp >= 0x1F3E0 && cp <= 0x1F3F0) return true;
    if (cp >= 0x1F3F4 && cp <= 0x1F3F4) return true;
    if (cp >= 0x1F3F8 && cp <= 0x1F43E) return true;
    if (cp >= 0x1F440 && cp <= 0x1F440) return true;
    if (cp >= 0x1F442 && cp <= 0x1F4FC) return true;
    if (cp >= 0x1F4FF && cp <= 0x1F53D) return true;
    if (cp >= 0x1F54B && cp <= 0x1F54E) return true;
    if (cp >= 0x1F550 && cp <= 0x1F567) return true;
    if (cp >= 0x1F57A && cp <= 0x1F57A) return true;
    if (cp >= 0x1F595 && cp <= 0x1F596) return true;
    if (cp >= 0x1F5A4 && cp <= 0x1F5A4) return true;
    if (cp >= 0x1F5FB && cp <= 0x1F64F) return true;
    if (cp >= 0x1F680 && cp <= 0x1F6C5) return true;
    if (cp >= 0x1F6CC && cp <= 0x1F6CC) return true;
    if (cp >= 0x1F6D0 && cp <= 0x1F6D2) return true;
    if (cp >= 0x1F6D5 && cp <= 0x1F6D7) return true;
    if (cp >= 0x1F6EB && cp <= 0x1F6EC) return true;
    if (cp >= 0x1F6F4 && cp <= 0x1F6FC) return true;
    if (cp >= 0x1F7E0 && cp <= 0x1F7EB) return true;
    if (cp >= 0x1F90C && cp <= 0x1F93A) return true;
    if (cp >= 0x1F93C && cp <= 0x1F945) return true;
    if (cp >= 0x1F947 && cp <= 0x1F9FF) return true;
    if (cp >= 0x1FA70 && cp <= 0x1FAFF) return true;
    if (cp >= 0x20000 && cp <= 0x2FFFD) return true;  // CJK Ext B..F
    if (cp >= 0x30000 && cp <= 0x3FFFD) return true;
    return false;
}

// Decodes one UTF-8 sequence starting at `i`. Returns its byte length (>= 1)
// and stores the code point in `cp`. A malformed sequence yields the leading
// byte as the code point and length 1, so scanning always makes progress.
std::size_t decode_utf8(std::string_view s, std::size_t i, char32_t& cp) {
    const unsigned char b0 = static_cast<unsigned char>(s[i]);
    if (b0 < 0x80) {
        cp = b0;
        return 1;
    }
    std::size_t len = 0;
    char32_t value = 0;
    if ((b0 & 0xE0) == 0xC0) {
        len = 2;
        value = b0 & 0x1F;
    } else if ((b0 & 0xF0) == 0xE0) {
        len = 3;
        value = b0 & 0x0F;
    } else if ((b0 & 0xF8) == 0xF0) {
        len = 4;
        value = b0 & 0x07;
    } else {
        cp = b0;
        return 1;
    }
    if (i + len > s.size()) {
        cp = b0;
        return 1;
    }
    for (std::size_t k = 1; k < len; ++k) {
        const unsigned char bk = static_cast<unsigned char>(s[i + k]);
        if ((bk & 0xC0) != 0x80) {
            cp = b0;
            return 1;
        }
        value = (value << 6) | (bk & 0x3F);
    }
    cp = value;
    return len;
}

}  // namespace

std::size_t display_width(std::string_view text) {
    std::size_t cells = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        char32_t cp = 0;
        const std::size_t len = decode_utf8(text, i, cp);
        if (is_zero_width(cp)) {
            // no cells
        } else if (is_wide(cp)) {
            cells += 2;
        } else {
            cells += 1;
        }
        i += len;
    }
    return cells;
}

std::string truncate_to_width(std::string_view text, std::size_t width) {
    std::string out;
    std::size_t cells = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        char32_t cp = 0;
        const std::size_t len = decode_utf8(text, i, cp);
        std::size_t w = 1;
        if (is_zero_width(cp)) {
            w = 0;
        } else if (is_wide(cp)) {
            w = 2;
        }
        if (cells + w > width) {
            break;  // a wide glyph that does not fit is dropped whole
        }
        out.append(text.substr(i, len));
        cells += w;
        i += len;
    }
    return out;
}

}  // namespace nanox::editor
