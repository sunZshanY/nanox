#pragma once

#include "nanox/Diagnostic.h"

#include <cstddef>
#include <string>
#include <vector>

namespace nanox {

// Result of analyzing one source file.
struct FileAnalysis {
    std::string path;
    std::size_t token_count = 0;
    std::vector<Diagnostic> diagnostics;
};

// Result of analyzing a whole project.
struct ProjectAnalysis {
    std::vector<FileAnalysis> files;
    double elapsed_seconds = 0.0;

    bool success() const;
    std::size_t total_tokens() const;
    std::size_t total_errors() const;
};

// A NanoX project: a directory containing *.nx source files.
//
// In Phase 1 the only analysis available is lexical analysis (the "build"
// action of the IDE runs this). Later phases extend analyze() to parsing and
// interpretation without changing the shape of the result.
class Project {
public:
    explicit Project(std::string root);

    const std::string& root() const { return root_; }

    // Walks up from `start` (a directory or file path) looking for a project
    // marker — `nanox.toml`, falling back to `CMakeLists.txt` — and returns
    // the directory that contains it. When no marker exists anywhere up the
    // tree, returns `start` itself (or its parent if `start` is a file). This
    // keeps the IDE from mistaking a build subdirectory (e.g. `build/`) for
    // the project root.
    static std::string discover_root(const std::string& start);

    // All *.nx files under root, recursively (sorted, build folders and
    // dot-directories skipped).
    std::vector<std::string> source_files() const;

    // Phase 1 "build": lexes every source file and reports token counts,
    // diagnostics, and elapsed time.
    ProjectAnalysis lex_all() const;

private:
    std::string root_;
};

}  // namespace nanox
