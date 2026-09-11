// NanoX command-line interface (Phase 1).
//
//   nanox                       open the IDE with the current directory as
//                               the project root and an untitled buffer
//   nanox <file.nx>             open that file; its parent directory is the
//                               project root (created on first save if it
//                               does not exist yet)
//   nanox <project-dir>         open the IDE on that project directory
//   nanox --mode=<mode> ...     start in an editing mode:
//                                 vim     modal, opens in NORMAL (default)
//                                 nano    always editing, ^O/^X/^W/^K/^U keys
//                                 hybrid  opens editing, ESC gives Vim NORMAL
//
// The mode can also be changed while running: F8 cycles the three modes, and
// ":set mode <vim|nano|hybrid>" selects one directly.
//
// The project root is discovered by walking up from the start directory for
// nanox.toml (or CMakeLists.txt), so `nanox` run from a build subdirectory
// still opens the real project instead of showing build artifacts.
//
// The CLI is a full-screen TUI (see docs/editor.md):
//   * project explorer (F2), multi-file editor with tabs
//   * F3 "build" = lexer pass over every *.nx file (Phase 1)
//   * F4 run (later phases), F5 REPL, F6 output, F1 help, ^Q quit
//   * the footer shows the keys that are live in the current editing mode

#include "nanox/Project.h"
#include "nanox/editor/Editor.h"
#include "nanox/editor/Keymap.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;
using nanox::editor::EditingMode;

namespace {

void print_usage(std::ostream& out) {
    out << "usage: nanox [--mode=vim|nano|hybrid] [file.nx | project-dir]\n"
           "\n"
           "  --mode=<mode>   editing mode to start in (default: vim)\n"
           "                  vim     modal editing, opens in NORMAL\n"
           "                  nano    GNU nano keys, always editing\n"
           "                  hybrid  opens editing, ESC gives Vim NORMAL\n"
           "  -h, --help      show this help\n";
}

}  // namespace

int main(int argc, char** argv) {
    EditingMode mode = EditingMode::Vim;
    std::string target;
    bool have_target = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(std::cout);
            return 0;
        }

        if (arg == "--mode" || arg.rfind("--mode=", 0) == 0) {
            std::string value;
            if (arg == "--mode") {
                if (i + 1 >= argc) {
                    std::cerr << "error: --mode needs a value\n";
                    print_usage(std::cerr);
                    return 2;
                }
                value = argv[++i];
            } else {
                value = arg.substr(std::string("--mode=").size());
            }

            const std::optional<EditingMode> parsed =
                nanox::editor::editing_mode_from_string(value);
            if (!parsed.has_value()) {
                std::cerr << "error: unknown editing mode: " << value
                          << " (expected vim, nano or hybrid)\n";
                return 2;
            }
            mode = *parsed;
            continue;
        }

        if (arg.size() > 1 && arg[0] == '-') {
            std::cerr << "error: unknown option: " << arg << '\n';
            print_usage(std::cerr);
            return 2;
        }

        if (have_target) {
            std::cerr << "error: only one file or directory may be given\n";
            print_usage(std::cerr);
            return 2;
        }
        target = arg;
        have_target = true;
    }

    std::string workspace;
    std::string open_file;
    std::string text;

    if (have_target) {
        const fs::path arg(target);
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

    nanox::editor::Editor editor(std::move(workspace), std::move(open_file),
                                 std::move(text), mode);
    return editor.run();
}
