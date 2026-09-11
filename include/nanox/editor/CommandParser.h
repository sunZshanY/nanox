#pragma once

#include "nanox/editor/Command.h"

#include <string>
#include <string_view>

namespace nanox::editor {

// A parsed ex-command line.
struct ParsedCommand {
    Command command = Command::None;
    std::string arg;     // ":w build/main.nx" -> "build/main.nx"
    bool force = false;  // a trailing '!'
    bool ok = false;     // false: not a recognized command
};

// Parses an ex-command line, with or without the leading ':'.
//
//   :w [file]            save (an argument makes it a save-as)
//   :q  :q!              quit / force quit
//   :wq  :x  :wq!        save and quit
//   :set mode <name>     switch editing mode (vim / nano / hybrid)
//
// Unknown lines come back with ok == false and Command::None so the caller can
// report them, rather than silently doing nothing. Pure string logic: it has no
// Editor dependency and is unit-tested directly.
ParsedCommand parse_ex_command(std::string_view line);

}  // namespace nanox::editor
