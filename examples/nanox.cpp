// NanoX command-line interface (Phase 1).
//
//   nanox                       open the IDE with the current directory as
//                               the project root and an untitled buffer
//   nanox <file.nx>             open that file; its parent directory is the
//                               project root (created on first save if it
//                               does not exist yet)
//   nanox <project-dir>         open the IDE on that project directory
//
// The project root is discovered by walking up from the start directory for
// nanox.toml (or CMakeLists.txt), so `nanox` run from a build subdirectory
// still opens the real project instead of showing build artifacts.
//
// The CLI is a full-screen TUI (see docs/editor.md):
//   * project explorer (F2), multi-file editor with tabs
//   * F3 "build" = lexer pass over every *.nx file (Phase 1)
//   * F4 run (later phases), F5 REPL, F6 output, F1 help, ^Q quit

#include "nanox/Project.h"
#include "nanox/editor/Editor.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cerr << "usage: nanox [file.nx | project-dir]\n";
        return 2;
    }

    std::string workspace;
    std::string open_file;
    std::string text;

    if (argc == 2) {
        const fs::path arg(argv[1]);
        std::error_code ec;
        if (fs::is_directory(arg, ec) && !ec) {
            workspace = nanox::Project::discover_root(arg.string());
        } else {
            open_file = arg.string();
            const std::string parent =
                arg.has_parent_path() ? arg.parent_path().string() : std::string(".");
            workspace = nanox::Project::discover_root(parent);
            std::ifstream in(open_file, std::ios::binary);
            if (!in) {
                if (fs::exists(arg, ec) && !ec) {
                    std::cerr << "error: cannot open file: " << open_file << '\n';
                    return 1;
                }
                // The file does not exist yet: start with an empty buffer;
                // Ctrl+S creates it.
            } else {
                std::ostringstream buffer;
                buffer << in.rdbuf();
                text = buffer.str();
            }
        }
    } else {
        std::error_code ec;
        workspace = nanox::Project::discover_root(fs::current_path(ec).string());
    }

    nanox::editor::Editor editor(std::move(workspace), std::move(open_file), std::move(text));
    return editor.run();
}
