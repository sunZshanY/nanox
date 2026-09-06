#include "nanox/Project.h"

#include "tempdir.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

using namespace nanox;

namespace fs = std::filesystem;

namespace {

nanox_test::TempDir make_project() {
    nanox_test::TempDir t("nanox_proj");
    t.write_file("main.nx", "fn main() {\n    println(\"hi\");\n}\n");
    t.write_file("math.nx", "fn add(a: int, b: int) -> int {\n    return a + b;\n}\n");
    t.write_file("bad.nx", "let @ = 1;\n");
    t.write_file("README.md", "not a source file\n");
    t.write_file("sub/nested.nx", "let x: int = 10;\n");
    t.write_file("build/ignored.nx", "should not be scanned\n");
    return t;
}

}  // namespace

NX_TEST_CASE(source_files_are_recursive_nx_only) {
    const nanox_test::TempDir t = make_project();
    const Project project(t.str());

    const std::vector<std::string> files = project.source_files();
    NX_CHECK_EQ(files.size(), static_cast<std::size_t>(4));  // main, math, bad, sub/nested

    for (const std::string& f : files) {
        NX_CHECK(f.size() >= 3 && f.substr(f.size() - 3) == ".nx");
        NX_CHECK(f.find("README.md") == std::string::npos);
        NX_CHECK(f.find("ignored.nx") == std::string::npos);
    }
}

NX_TEST_CASE(source_files_are_sorted) {
    const nanox_test::TempDir t = make_project();
    const Project project(t.str());
    const std::vector<std::string> files = project.source_files();
    for (std::size_t i = 1; i < files.size(); ++i) {
        NX_CHECK(files[i - 1] < files[i]);
    }
}

NX_TEST_CASE(lex_all_analyzes_every_file) {
    const nanox_test::TempDir t = make_project();
    const Project project(t.str());

    const ProjectAnalysis result = project.lex_all();
    NX_CHECK_EQ(result.files.size(), static_cast<std::size_t>(4));
    NX_CHECK(result.total_tokens() > 0);
    NX_CHECK(result.elapsed_seconds >= 0.0);

    // bad.nx contains an invalid character -> exactly one error overall.
    NX_CHECK_EQ(result.total_errors(), static_cast<std::size_t>(1));
    NX_CHECK(!result.success());

    bool found_error_file = false;
    for (const FileAnalysis& file : result.files) {
        if (file.path.find("bad.nx") != std::string::npos) {
            found_error_file = true;
            NX_CHECK_EQ(file.diagnostics.size(), static_cast<std::size_t>(1));
            NX_CHECK_EQ(file.diagnostics[0].location.line, static_cast<std::uint32_t>(1));
            NX_CHECK_EQ(file.diagnostics[0].location.column, static_cast<std::uint32_t>(5));
        } else {
            NX_CHECK(file.diagnostics.empty());
        }
    }
    NX_CHECK(found_error_file);
}

NX_TEST_CASE(lex_all_succeeds_without_errors) {
    nanox_test::TempDir t("nanox_proj_ok");
    t.write_file("good.nx", "let x: int = 10;\n");
    const Project project(t.str());

    const ProjectAnalysis result = project.lex_all();
    NX_CHECK(result.success());
    NX_CHECK_EQ(result.total_errors(), static_cast<std::size_t>(0));
    NX_CHECK_EQ(result.files.size(), static_cast<std::size_t>(1));
    NX_CHECK_EQ(result.files[0].token_count, static_cast<std::size_t>(7));  // let x : int = 10 ;
}

NX_TEST_CASE(missing_root_yields_empty_analysis) {
    const Project project("nanox_no_such_project_xyz");
    NX_CHECK(project.source_files().empty());
    const ProjectAnalysis result = project.lex_all();
    NX_CHECK(result.success());
    NX_CHECK_EQ(result.total_tokens(), static_cast<std::size_t>(0));
}

NX_TEST_CASE(discover_root_finds_nanox_toml) {
    nanox_test::TempDir t("nanox_root");
    t.write_file("nanox.toml", "[project]\n");
    t.write_file("src/main.nx", "fn main() {}\n");
    t.make_dir("src/deep");

    NX_CHECK_EQ(fs::path(Project::discover_root(t.str() + "/src/deep")), fs::path(t.str()));
    // Starting at a file path also walks up from its directory.
    NX_CHECK_EQ(fs::path(Project::discover_root(t.str() + "/src/main.nx")),
                fs::path(t.str()));
}

NX_TEST_CASE(discover_root_falls_back_to_cmake) {
    nanox_test::TempDir t("nanox_cmake");
    t.write_file("CMakeLists.txt", "project(nanox)\n");
    t.make_dir("build/examples");

    NX_CHECK_EQ(fs::path(Project::discover_root(t.str() + "/build/examples")),
                fs::path(t.str()));
}

NX_TEST_CASE(discover_root_nearest_marker_wins) {
    nanox_test::TempDir t("nanox_nested");
    t.write_file("nanox.toml", "[project]\n");
    t.write_file("sub/CMakeLists.txt", "project(sub)\n");
    t.write_file("sub/src/main.nx", "fn main() {}\n");

    // The walk starts at sub/src and finds sub/CMakeLists.txt before ever
    // reaching the nanox.toml one level up.
    NX_CHECK_EQ(fs::path(Project::discover_root(t.str() + "/sub/src")),
                fs::path(t.str() + "/sub"));
}
