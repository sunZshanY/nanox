#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace nanox::editor {

// Every action the editor can perform, independent of the key that triggers it.
//
// Design note: keys are never compared against literals outside the Keymap.
// A key press is resolved to a Command by Keymap, an ex-command line (":w") is
// resolved to a Command by CommandParser, and Editor::execute() switches on the
// Command alone. That indirection is what lets a future plugin layer rebind
// keys -- or invoke commands by name -- without touching the editor core:
//
//     nanox.command("save")        ->  command_from_name("save")
//     nanox.keymap("normal", "Ctrl+S", "save")
//
// Command::None means "nothing to do"; it is never bound to a key.
enum class Command {
    None,

    // --- files, tabs and the application ------------------------------------
    Save,             // :w          write the current buffer
    WriteOut,         // ^O          "File Name to Write:" (always prompts)
    Quit,             // :q  ^Q      leave the editor (confirms if dirty)
    ForceQuit,        // :q!         leave even if dirty
    SaveAndQuit,      // :wq         write, then leave
    CloseTab,         // ^X          close the tab (quits when it was the last)
    NextTab,          // ^T
    CycleEditingMode, // F8          Vim -> Nano -> Hybrid -> Vim
    SetEditingMode,   // :set mode   argument names the mode (vim/nano/hybrid)

    // --- panels --------------------------------------------------------------
    Help,             // F1  ^G
    Build,            // F3  ^B
    Run,              // F4  ^R      (Nano/Hybrid only; Vim uses ^R for Redo)
    Repl,             // F5
    ToggleOutput,     // F6
    ToggleExplorer,   // F2

    // --- editing (shared by Nano and Vim) ------------------------------------
    Undo,             // Alt+U  u
    Redo,             // Alt+E  ^R
    CutLine,          // ^K  dd     cut the current line into the clipboard
    CopyLine,         // Alt+6  yy  copy the current line into the clipboard
    Paste,            // ^U         paste the clipboard at the cursor
    Search,           // ^W         "Search:"
    Replace,          // ^\         "Search:" + "Replace with:"
    GoToLine,         // ^_         "line,column"

    // --- mode transitions ----------------------------------------------------
    EnterNormal,      // ESC        INSERT -> NORMAL (also cancels a pending key)
    VimInsert,        // i
    VimAppend,        // a
    VimInsertLineStart,  // I
    VimAppendLineEnd,    // A
    VimOpenBelow,     // o
    VimOpenAbove,     // O
    EnterCommandMode, // :  ^P      open the ex-command prompt

    // --- cursor motion -------------------------------------------------------
    MoveLeft,         // h  Left
    MoveDown,         // j  Down
    MoveUp,           // k  Up
    MoveRight,        // l  Right
    LineStart,        // 0
    LineEnd,          // $

    // --- editing operators ---------------------------------------------------
    DeleteChar,       // x
    DeleteToEnd,      // D
    DeleteLine,       // dd
    YankLine,         // yy
    PasteAfter,       // p
    PasteBefore,      // P
};

// Stable lowercase name for a command ("save", "delete_line"). Names are the
// plugin-facing identifier, so they must not change once released.
std::string_view command_name(Command command);

// Reverse lookup, used by the keymap notation ("Ctrl+S" -> "save") and by the
// future `nanox.command(...)` API. Returns std::nullopt for unknown names.
std::optional<Command> command_from_name(std::string_view name);

}  // namespace nanox::editor
