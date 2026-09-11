#include "nanox/editor/Command.h"

namespace nanox::editor {
namespace {

// One table drives both directions, so a name can never drift from its command.
struct Entry {
    Command command;
    std::string_view name;
};

constexpr Entry kCommands[] = {
    {Command::None, "none"},

    {Command::Save, "save"},
    {Command::WriteOut, "write_out"},
    {Command::Quit, "quit"},
    {Command::ForceQuit, "force_quit"},
    {Command::SaveAndQuit, "save_and_quit"},
    {Command::CloseTab, "close_tab"},
    {Command::NextTab, "next_tab"},
    {Command::CycleEditingMode, "cycle_editing_mode"},
    {Command::SetEditingMode, "set_editing_mode"},

    {Command::Help, "help"},
    {Command::Build, "build"},
    {Command::Run, "run"},
    {Command::Repl, "repl"},
    {Command::ToggleOutput, "toggle_output"},
    {Command::ToggleExplorer, "toggle_explorer"},

    {Command::Undo, "undo"},
    {Command::Redo, "redo"},
    {Command::CutLine, "cut_line"},
    {Command::CopyLine, "copy_line"},
    {Command::Paste, "paste"},
    {Command::Search, "search"},
    {Command::Replace, "replace"},
    {Command::GoToLine, "go_to_line"},

    {Command::EnterNormal, "enter_normal"},
    {Command::VimInsert, "vim_insert"},
    {Command::VimAppend, "vim_append"},
    {Command::VimInsertLineStart, "vim_insert_line_start"},
    {Command::VimAppendLineEnd, "vim_append_line_end"},
    {Command::VimOpenBelow, "vim_open_below"},
    {Command::VimOpenAbove, "vim_open_above"},
    {Command::EnterCommandMode, "enter_command_mode"},

    {Command::MoveLeft, "move_left"},
    {Command::MoveDown, "move_down"},
    {Command::MoveUp, "move_up"},
    {Command::MoveRight, "move_right"},
    {Command::LineStart, "line_start"},
    {Command::LineEnd, "line_end"},

    {Command::DeleteChar, "delete_char"},
    {Command::DeleteToEnd, "delete_to_end"},
    {Command::DeleteLine, "delete_line"},
    {Command::YankLine, "yank_line"},
    {Command::PasteAfter, "paste_after"},
    {Command::PasteBefore, "paste_before"},
};

}  // namespace

std::string_view command_name(Command command) {
    for (const Entry& entry : kCommands) {
        if (entry.command == command) {
            return entry.name;
        }
    }
    return "none";
}

std::optional<Command> command_from_name(std::string_view name) {
    for (const Entry& entry : kCommands) {
        if (entry.name == name) {
            return entry.command;
        }
    }
    return std::nullopt;
}

}  // namespace nanox::editor
