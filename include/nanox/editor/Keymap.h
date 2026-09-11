#pragma once

#include "nanox/editor/Command.h"
#include "nanox/editor/Terminal.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nanox::editor {

// How the editor interprets keys. Selected with `--mode=` or at runtime with F8
// and `:set mode <name>`.
//   Vim     - modal: opens in NORMAL, `i`/`a`/`o`/`O` enter INSERT.
//   Nano    - never leaves the editor: every key edits, ^O/^X/^W/^K/^U do the
//             GNU nano actions. There is no NORMAL state at all.
//   Hybrid  - opens in INSERT with plain typing, but ESC drops into the same
//             NORMAL state Vim uses; INSERT additionally accepts nano's ^O/^W/^K/^U.
enum class EditingMode { Vim, Nano, Hybrid };

// The state inside an EditingMode. Nano is always Insert (its status line reads
// "EDITOR" instead); Command is the ":" / Ctrl+P ex-command prompt.
enum class EditorMode { Normal, Insert, Command };

std::optional<EditingMode> editing_mode_from_string(std::string_view text);
std::string_view editing_mode_name(EditingMode mode);

// The state that EditorMode reports in the title bar: "NORMAL" / "INSERT" /
// "COMMAND", or "EDITOR" for nano (which has no modes to report).
std::string_view editor_mode_name(EditingMode editing, EditorMode mode);

// A single key press, normalized so that it can be compared and stored.
//
// Ctrl and Alt combinations are folded onto Kind::Char plus a modifier flag
// (Ctrl+S is {Char, 's', ctrl}, not Kind::Ctrl), so one lookup path handles
// plain, control and meta keys alike. That is why Keymap never sees Key::Kind::Ctrl.
struct KeyChord {
    Key::Kind kind = Key::Kind::None;
    char ch = 0;
    bool ctrl = false;
    bool alt = false;
    unsigned param = 0;  // Kind::Function: F-key number

    // Converts a terminal event into a chord. Ctrl/Alt are normalized as above.
    static KeyChord from_key(const Key& key);

    // The notation accepted by chord_from_string(): "Ctrl+S", "Alt+U", "F3",
    // "Escape", "i". Round-trips through chord_from_string().
    std::string to_string() const;

    bool operator==(const KeyChord& other) const;
    bool operator!=(const KeyChord& other) const { return !(*this == other); }
    bool operator<(const KeyChord& other) const;
};

// Parses a single chord in the notation above. Returns nullopt when malformed.
// Note: multi-key sequences ("dd") are NOT expressible here -- see
// Keymap::bind_sequence, which takes the chords explicitly so the notation can
// stay unambiguous.
std::optional<KeyChord> chord_from_string(std::string_view text);

// A chord (or chords) bound to a command in one editing state.
struct KeyBinding {
    EditingMode editing = EditingMode::Vim;
    EditorMode editor = EditorMode::Normal;
    std::vector<KeyChord> chords;
    Command command = Command::None;
};

// Maps keys to Commands. Owns every default binding and is the single place a
// binding can be changed, so nothing downstream hardcodes a key:
//
//     keymap.bind(EditingMode::Vim, EditorMode::Normal,
//                 chord_from_string("Ctrl+S").value(), Command::Save);
//
// This is the surface a future `nanox.keymap("normal", "Ctrl+S", "save")`
// plugin API would drive; the string overload below is that entry point.
class Keymap {
public:
    enum class Match {
        None,      // no binding: the caller decides what the key means
        Pending,   // a prefix of a longer sequence ("d" of "dd")
        Complete,  // resolved; `out` holds the command
    };

    Keymap();  // installs the default bindings below

    void bind(EditingMode editing, EditorMode editor, KeyChord chord, Command command);
    void bind_sequence(EditingMode editing, EditorMode editor,
                       std::vector<KeyChord> chords, Command command);

    // Plugin-facing overloads that take names instead of enums, e.g.
    // bind("normal", "Ctrl+S", "save"). Return false if a name is unknown.
    bool bind(std::string_view editor_mode, std::string_view chord_text,
              std::string_view command_text);

    void unbind(EditingMode editing, EditorMode editor, const std::vector<KeyChord>& chords);

    // Feeds one chord. Pending state is kept internally and cleared by
    // reset_pending() (on ESC, on an unrelated key, or when the mode changes).
    Match feed(EditingMode editing, EditorMode editor, KeyChord chord, Command& out);
    void reset_pending();

    const std::vector<KeyBinding>& bindings() const { return bindings_; }

    // Every binding live in one state, as {key notation, command name} pairs
    // sorted by key. render_help() builds the help overlay from this so it can
    // never drift from the real bindings.
    std::vector<std::pair<std::string, std::string>> bindings_for(EditingMode editing,
                                                                  EditorMode editor) const;

    // The one-line hint shown in the footer for a state. Rendering calls this;
    // the footer text is never written out in Editor.cpp.
    static std::string footer_hint(EditingMode editing, EditorMode editor);

private:
    std::vector<KeyBinding> bindings_;
    std::vector<KeyChord> pending_;
};

}  // namespace nanox::editor
