#include "nanox/Diagnostic.h"

#include "test_framework.hpp"

#include <sstream>
#include <string>

using namespace nanox;

NX_TEST_CASE(diagnostic_engine_starts_clean) {
    DiagnosticEngine diag("test.nx");
    NX_CHECK(!diag.has_errors());
    NX_CHECK(diag.diagnostics().empty());
}

NX_TEST_CASE(error_is_tracked) {
    DiagnosticEngine diag("test.nx");
    diag.error((SourceLocation{3, 4, 0}), "boom");
    NX_CHECK(diag.has_errors());
    NX_CHECK_EQ(diag.diagnostics().size(), static_cast<std::size_t>(1));
    NX_CHECK_EQ(diag.diagnostics()[0].message, std::string("boom"));
    NX_CHECK_EQ(diag.diagnostics()[0].location, (SourceLocation{3, 4, 0}));
}

NX_TEST_CASE(warning_does_not_set_error_flag) {
    DiagnosticEngine diag("test.nx");
    diag.warning((SourceLocation{1, 1, 0}), "deprecated");
    NX_CHECK(!diag.has_errors());
    NX_CHECK_EQ(diag.diagnostics().size(), static_cast<std::size_t>(1));
}

NX_TEST_CASE(print_all_contains_file_line_column) {
    DiagnosticEngine diag("main.nx");
    diag.error((SourceLocation{5, 10, 0}), "unexpected token");
    diag.warning((SourceLocation{2, 1, 0}), "something odd");

    std::ostringstream os;
    diag.print_all(os);

    const std::string out = os.str();
    NX_CHECK(out.find("main.nx:5:10: error: unexpected token") != std::string::npos);
    NX_CHECK(out.find("main.nx:2:1: warning: something odd") != std::string::npos);
}
