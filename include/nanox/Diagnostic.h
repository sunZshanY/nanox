#pragma once

#include "nanox/SourceLocation.h"

#include <ostream>
#include <string>
#include <vector>

namespace nanox {

enum class Severity {
    Error,
    Warning,
};

// A single diagnostic message tied to a source location.
struct Diagnostic {
    Severity severity = Severity::Error;
    SourceLocation location{};
    std::string message;
};

// Collects diagnostics for one translation unit and knows how to render them.
//
// Design note: the engine owns a reference to the source filename so that every
// printed message is self-contained ("file:line:column: error: ..."). It is
// intentionally independent of the Lexer so that future phases (Parser, type
// checker, ...) can reuse it without coupling.
class DiagnosticEngine {
public:
    explicit DiagnosticEngine(std::string filename);

    void error(SourceLocation loc, std::string message);
    void warning(SourceLocation loc, std::string message);

    bool has_errors() const;

    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

    // Prints all diagnostics in source order to `os`, one per line.
    void print_all(std::ostream& os) const;

private:
    std::string filename_;
    std::vector<Diagnostic> diagnostics_;
};

}  // namespace nanox
