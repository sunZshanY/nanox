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

NX_TEST_CASE(resolve_source_path_appends_nx_to_bare_names) {
    const nanox_test::TempDir t = make_project();
    // "main" exists as main.nx on disk: a bare name resolves to the source.
    NX_CHECK_EQ(Project::resolve_source_path((t.path / "main").string()),
                (t.path / "main.nx").string());
    // A missing bare name is still a NanoX source (created on save).
    NX_CHECK_EQ(Project::resolve_source_path((t.path / "newfile").string()),
                (t.path / "newfile.nx").string());
}

NX_TEST_CASE(resolve_source_path_keeps_existing_paths_and_extensions) {
    const nanox_test::TempDir t = make_project();
    // An existing path is returned as-is.
    NX_CHECK_EQ(Project::resolve_source_path((t.path / "main.nx").string()),
                (t.path / "main.nx").string());
    // A non-.nx extension is respected: no magic substitution.
    NX_CHECK_EQ(Project::resolve_source_path((t.path / "notes.txt").string()),
                (t.path / "notes.txt").string());
}

NX_TEST_CASE(resolve_source_path_prefers_an_existing_extensionless_file) {
    nanox_test::TempDir t("nanox_resolve");
    t.write_file("plain", "x");
    NX_CHECK_EQ(Project::resolve_source_path((t.path / "plain").string()),
                (t.path / "plain").string());
}

NX_TEST_CASE(discover_root_does_not_treat_cmakelists_as_a_marker) {
    // Walking up to an unrelated parent that happens to contain a
    // CMakeLists.txt must not attach the editor to it (that parent could be a
    // home directory with a huge tree); without nanox.toml the start
    // directory itself is the project root.
    nanox_test::TempDir t("nanox_nomarker");
    t.write_file("CMakeLists.txt", "cmake_minimum_required(VERSION 3.20)\n");
    t.write_file("hello.nx", "let x = 1;\n");
    NX_CHECK_EQ(Project::discover_root(t.str()), t.path.string());
}

NX_TEST_CASE(discover_root_of_a_missing_path_stays_in_its_parent) {
    const nanox_test::TempDir t = make_project();  // no nanox.toml inside
    NX_CHECK_EQ(Project::discover_root((t.path / "missing").string()), t.path.string());
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

NX_TEST_CASE(discover_root_nearest_nanox_toml_wins) {
    nanox_test::TempDir t("nanox_nested");
    t.write_file("nanox.toml", "[project]\n");
    t.write_file("sub/nanox.toml", "[project]\n");
    t.write_file("sub/src/main.nx", "fn main() {}\n");

    // The walk starts at sub/src and finds sub/nanox.toml before the one in t.
    NX_CHECK_EQ(fs::path(Project::discover_root(t.str() + "/sub/src")),
                fs::path(t.str() + "/sub"));
}
