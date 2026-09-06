#include "nanox/Diagnostic.h"

#include <utility>

namespace nanox {

DiagnosticEngine::DiagnosticEngine(std::string filename)
    : filename_(std::move(filename)) {}

void DiagnosticEngine::error(SourceLocation loc, std::string message) {
    diagnostics_.push_back(Diagnostic{Severity::Error, loc, std::move(message)});
}

void DiagnosticEngine::warning(SourceLocation loc, std::string message) {
    diagnostics_.push_back(Diagnostic{Severity::Warning, loc, std::move(message)});
}

bool DiagnosticEngine::has_errors() const {
    for (const Diagnostic& d : diagnostics_) {
        if (d.severity == Severity::Error) {
            return true;
        }
    }
    return false;
}

void DiagnosticEngine::print_all(std::ostream& os) const {
    for (const Diagnostic& d : diagnostics_) {
        os << filename_ << ':' << d.location.line << ':' << d.location.column << ": ";
        switch (d.severity) {
            case Severity::Error:
                os << "error";
                break;
            case Severity::Warning:
                os << "warning";
                break;
        }
        os << ": " << d.message << '\n';
    }
}

}  // namespace nanox
