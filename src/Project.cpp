#include "nanox/Project.h"

#include "nanox/FileTree.h"
#include "nanox/Lexer.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace nanox {

namespace fs = std::filesystem;

Project::Project(std::string root) : root_(std::move(root)) {}

std::string Project::resolve_source_path(const std::string& path) {
    std::error_code ec;
    if (fs::exists(fs::path(path), ec) && !ec) {
        return path;
    }
    const fs::path candidate(path);
    if (!candidate.has_extension()) {
        return candidate.string() + ".nx";
    }
    return path;
}

std::string Project::discover_root(const std::string& start) {
    fs::path dir(start);
    std::error_code ec;
    if (!fs::is_directory(dir, ec) || ec) {
        dir = dir.parent_path();  // `start` is a file: begin at its folder
    }
    if (dir.empty()) {
        dir = ".";
    }

    fs::path current = fs::absolute(dir, ec);
    if (ec) {
        return start;
    }

    // `nanox.toml` is the one and only project marker. We deliberately do NOT
    // treat an unrelated CMakeLists.txt as a marker: walking up from an
    // arbitrary directory would otherwise attach the editor to whatever parent
    // happens to contain one (a home directory, for example), and scanning
    // that as a "project" can take a very long time.
    const fs::path fallback = current;  // no marker anywhere: scan just this folder
    for (;;) {
        std::error_code marker_ec;
        if (fs::exists(current / "nanox.toml", marker_ec)) {
            return current.string();
        }
        const fs::path parent = current.parent_path();
        if (parent.empty() || parent == current) {
            break;
        }
        current = parent;
    }
    return fallback.string();
}

std::vector<std::string> Project::source_files() const {
    std::vector<std::string> files;

    std::error_code ec;
    fs::recursive_directory_iterator it(
        root_, fs::directory_options::skip_permission_denied, ec);
    const fs::recursive_directory_iterator end;
    for (; !ec && it != end; it.increment(ec)) {
        const fs::path& path = it->path();

        if (is_ignored_entry(path.filename().string())) {
            // NOTE: depth() is 0 for the root's direct children, so ignored
            // top-level directories must not be guarded by a depth check.
            std::error_code dir_ec;
            if (it->is_directory(dir_ec) && !dir_ec) {
                it.disable_recursion_pending();
            }
            continue;
        }

        std::error_code file_ec;
        if (it->is_regular_file(file_ec) && !file_ec && path.extension() == ".nx") {
            files.push_back(path.string());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

ProjectAnalysis Project::lex_all() const {
    ProjectAnalysis result;
    const auto start = std::chrono::steady_clock::now();

    for (const std::string& path : source_files()) {
        FileAnalysis file;
        file.path = path;

        std::ifstream in(path, std::ios::binary);
        std::ostringstream buffer;
        buffer << in.rdbuf();
        const std::string text = buffer.str();

        DiagnosticEngine diagnostics(path);
        Lexer lexer(text, path, diagnostics);
        while (true) {
            const Token token = lexer.next();
            if (token.is_eof()) {
                break;
            }
            ++file.token_count;
        }
        file.diagnostics = diagnostics.diagnostics();

        result.files.push_back(std::move(file));
    }

    const auto end = std::chrono::steady_clock::now();
    result.elapsed_seconds = std::chrono::duration<double>(end - start).count();
    return result;
}

bool ProjectAnalysis::success() const {
    return total_errors() == 0;
}

std::size_t ProjectAnalysis::total_tokens() const {
    std::size_t total = 0;
    for (const FileAnalysis& file : files) {
        total += file.token_count;
    }
    return total;
}

std::size_t ProjectAnalysis::total_errors() const {
    std::size_t total = 0;
    for (const FileAnalysis& file : files) {
        for (const Diagnostic& d : file.diagnostics) {
            if (d.severity == Severity::Error) {
                ++total;
            }
        }
    }
    return total;
}

}  // namespace nanox
