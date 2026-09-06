#pragma once

#include <cstdint>
#include <ostream>

namespace nanox {

// A single position in the source text.
//
// Design note:
//   * line/column are 1-based (the first character is at 1:1), which is what
//     humans and editors expect in diagnostics.
//   * offset is a 0-based byte offset into the source buffer. It is kept here
//     so that later phases (error recovery, caret rendering) can map a location
//     back to the exact source text without re-scanning.
struct SourceLocation {
    std::uint32_t line = 1;
    std::uint32_t column = 1;
    std::uint32_t offset = 0;
};

inline bool operator==(const SourceLocation& a, const SourceLocation& b) {
    return a.line == b.line && a.column == b.column && a.offset == b.offset;
}

inline bool operator!=(const SourceLocation& a, const SourceLocation& b) {
    return !(a == b);
}

// Prints as "line:column" (e.g. "3:14"). This is the canonical human-readable
// form used across all NanoX diagnostics.
inline std::ostream& operator<<(std::ostream& os, const SourceLocation& loc) {
    return os << loc.line << ':' << loc.column;
}

}  // namespace nanox
