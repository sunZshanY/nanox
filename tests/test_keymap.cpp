#include "nanox/editor/Command.h"
#include "nanox/editor/Keymap.h"

#include "test_framework.hpp"

#include <optional>
#include <string>

using namespace nanox::editor;

namespace {

KeyChord chord(const char* text) {
    const std::optional<KeyChord> parsed = chord_from_string(text);
    NX_CHECK(parsed.has_value());
    return *parsed;
}

// Resolves one key press in a given state, asserting it completes.
Command resolve(EditingMode editing, EditorMode editor, const KeyChord& key) {
    Keymap keymap;
    Command command = Command::None;
    const Keymap::Match match = keymap.feed(editing, editor, key, command);
    NX_CHECK(match == Keymap::Match::Complete);
    return command;
}

Command press(EditingMode editing, EditorMode editor, const char* text) {
    return resolve(editing, editor, chord(text));
}

}  // namespace

// --- chord notation ---------------------------------------------------------

NX_TEST_CASE(chord_parses_modifiers_and_named_keys) {
    const KeyChord ctrl_s = chord("Ctrl+S");
    NX_CHECK(ctrl_s.ctrl);
    NX_CHECK(!ctrl_s.alt);
    NX_CHECK_EQ(ctrl_s.ch, 's');

    const KeyChord alt_u = chord("Alt+U");
    NX_CHECK(alt_u.alt);
    NX_CHECK_EQ(alt_u.ch, 'u');

    NX_CHECK(chord("Escape").kind == Key::Kind::Escape);
    NX_CHECK(chord("Tab").kind == Key::Kind::Tab);
    NX_CHECK(chord("Enter").kind == Key::Kind::Enter);

    const KeyChord f3 = chord("F3");
    NX_CHECK(f3.kind == Key::Kind::Function);
    NX_CHECK_EQ(f3.param, 3u);
}

NX_TEST_CASE(chord_notation_is_case_insensitive_and_accepts_caret) {
    NX_CHECK(chord("ctrl+s") == chord("Ctrl+S"));
    NX_CHECK(chord("^S") == chord("Ctrl+S"));
    NX_CHECK(chord("^s") == chord("Ctrl+S"));
    NX_CHECK(chord("M-u") == chord("Alt+U"));
}

NX_TEST_CASE(chord_round_trips_through_notation) {
    const char* texts[] = {"Ctrl+S", "Alt+U", "F5", "Escape", "Tab", "Enter"};
    for (const char* text : texts) {
        const KeyChord original = chord(text);
        const std::optional<KeyChord> reparsed = chord_from_string(original.to_string());
        NX_CHECK(reparsed.has_value());
        NX_CHECK(*reparsed == original);
    }
}

NX_TEST_CASE(chord_rejects_garbage) {
    NX_CHECK(!chord_from_string("").has_value());
    NX_CHECK(!chord_from_string("Ctrl+").has_value());
    NX_CHECK(!chord_from_string("NotAKey").has_value());
    NX_CHECK(!chord_from_string("F99").has_value());
}

NX_TEST_CASE(plain_char_keeps_its_case) {
    // 'S' and 's' are different keys unless a modifier makes them one.
    NX_CHECK(chord("S") != chord("s"));
    NX_CHECK_EQ(chord("S").ch, 'S');
}

// --- Key -> KeyChord --------------------------------------------------------

NX_TEST_CASE(terminal_keys_normalize_to_chords) {
    Key key;
    key.kind = Key::Kind::Ctrl;
    key.ch = 's';
    const KeyChord from_ctrl = KeyChord::from_key(key);
    NX_CHECK(from_ctrl.ctrl);
    NX_CHECK(from_ctrl.kind == Key::Kind::Char);
    NX_CHECK(from_ctrl == chord("Ctrl+S"));

    Key alt_key;
    alt_key.kind = Key::Kind::Alt;
    alt_key.ch = 'u';
    NX_CHECK(KeyChord::from_key(alt_key) == chord("Alt+U"));

    Key plain;
    plain.kind = Key::Kind::Char;
    plain.ch = 'i';
    NX_CHECK(KeyChord::from_key(plain) == chord("i"));
}

NX_TEST_CASE(ctrl_punctuation_chords_are_distinct) {
    // nano's ^\ and ^_ need these; they are not letters, so they must not
    // collide with Ctrl+A..Ctrl+Z.
    NX_CHECK(chord("Ctrl+\\").ctrl);
    NX_CHECK_EQ(chord("Ctrl+\\").ch, '\\');
    NX_CHECK_EQ(chord("Ctrl+_").ch, '_');
    NX_CHECK(chord("Ctrl+\\") != chord("Ctrl+W"));
}

// --- default bindings -------------------------------------------------------

NX_TEST_CASE(vim_normal_bindings) {
    const EditingMode vim = EditingMode::Vim;
    const EditorMode normal = EditorMode::Normal;
    NX_CHECK(press(vim, normal, "i") == Command::VimInsert);
    NX_CHECK(press(vim, normal, "a") == Command::VimAppend);
    NX_CHECK(press(vim, normal, "o") == Command::VimOpenBelow);
    NX_CHECK(press(vim, normal, "O") == Command::VimOpenAbove);
    NX_CHECK(press(vim, normal, "h") == Command::MoveLeft);
    NX_CHECK(press(vim, normal, "j") == Command::MoveDown);
    NX_CHECK(press(vim, normal, "k") == Command::MoveUp);
    NX_CHECK(press(vim, normal, "l") == Command::MoveRight);
    NX_CHECK(press(vim, normal, "x") == Command::DeleteChar);
    NX_CHECK(press(vim, normal, "p") == Command::PasteAfter);
    NX_CHECK(press(vim, normal, "P") == Command::PasteBefore);
    NX_CHECK(press(vim, normal, "u") == Command::Undo);
    NX_CHECK(press(vim, normal, "Escape") == Command::EnterNormal);
}

NX_TEST_CASE(vim_normal_ctrl_r_is_redo_not_run) {
    // Documented conflict: the Vim key list gives ^R to Redo, so Run lives on
    // F4 and the Vim footer must not advertise "^R Run".
    NX_CHECK(press(EditingMode::Vim, EditorMode::Normal, "Ctrl+R") == Command::Redo);
    const std::string vim_footer =
        Keymap::footer_hint(EditingMode::Vim, EditorMode::Normal);
    NX_CHECK(vim_footer.find("Redo") != std::string::npos);
    NX_CHECK(vim_footer.find("^R Run") == std::string::npos);
}

NX_TEST_CASE(nano_ctrl_r_is_run_because_nano_leaves_it_free) {
    NX_CHECK(press(EditingMode::Nano, EditorMode::Insert, "Ctrl+R") == Command::Run);
}

NX_TEST_CASE(nano_bindings_match_the_spec) {
    const EditingMode nano = EditingMode::Nano;
    const EditorMode editing = EditorMode::Insert;
    NX_CHECK(press(nano, editing, "Ctrl+O") == Command::WriteOut);
    NX_CHECK(press(nano, editing, "Ctrl+X") == Command::CloseTab);
    NX_CHECK(press(nano, editing, "Ctrl+W") == Command::Search);
    NX_CHECK(press(nano, editing, "Ctrl+\\") == Command::Replace);
    NX_CHECK(press(nano, editing, "Ctrl+K") == Command::CutLine);
    NX_CHECK(press(nano, editing, "Ctrl+U") == Command::Paste);
    NX_CHECK(press(nano, editing, "Ctrl+G") == Command::Help);
    NX_CHECK(press(nano, editing, "Ctrl+_") == Command::GoToLine);
    NX_CHECK(press(nano, editing, "Alt+U") == Command::Undo);
    NX_CHECK(press(nano, editing, "Alt+E") == Command::Redo);
    NX_CHECK(press(nano, editing, "Alt+6") == Command::CopyLine);
}

NX_TEST_CASE(hybrid_insert_takes_the_nano_keys_and_escape) {
    const EditingMode hybrid = EditingMode::Hybrid;
    const EditorMode insert = EditorMode::Insert;
    NX_CHECK(press(hybrid, insert, "Escape") == Command::EnterNormal);
    NX_CHECK(press(hybrid, insert, "Ctrl+O") == Command::WriteOut);
    NX_CHECK(press(hybrid, insert, "Ctrl+W") == Command::Search);
    NX_CHECK(press(hybrid, insert, "Ctrl+K") == Command::CutLine);
    NX_CHECK(press(hybrid, insert, "Ctrl+U") == Command::Paste);
}

NX_TEST_CASE(hybrid_normal_is_the_vim_normal_state) {
    NX_CHECK(press(EditingMode::Hybrid, EditorMode::Normal, "i") == Command::VimInsert);
    NX_CHECK(press(EditingMode::Hybrid, EditorMode::Normal, "x") == Command::DeleteChar);
}

NX_TEST_CASE(close_tab_key_differs_between_nano_and_vim) {
    // ^W means Search in nano but stays Close-tab in Vim.
    NX_CHECK(press(EditingMode::Nano, EditorMode::Insert, "Ctrl+W") == Command::Search);
    NX_CHECK(press(EditingMode::Vim, EditorMode::Normal, "Ctrl+W") == Command::CloseTab);
    // ^X closes everywhere.
    NX_CHECK(press(EditingMode::Vim, EditorMode::Normal, "Ctrl+X") == Command::CloseTab);
}

NX_TEST_CASE(vim_insert_leaves_nano_keys_unbound) {
    // The Vim INSERT footer lists only ESC / ^S / ^Q, so ^K must NOT cut there.
    Keymap keymap;
    Command command = Command::None;
    const Keymap::Match match = keymap.feed(EditingMode::Vim, EditorMode::Insert,
                                            chord("Ctrl+K"), command);
    NX_CHECK(match == Keymap::Match::None);
}

// --- multi-key sequences ----------------------------------------------------

NX_TEST_CASE(dd_and_yy_are_two_key_sequences) {
    Keymap keymap;
    Command command = Command::None;

    NX_CHECK(keymap.feed(EditingMode::Vim, EditorMode::Normal, chord("d"), command) ==
             Keymap::Match::Pending);
    NX_CHECK(keymap.feed(EditingMode::Vim, EditorMode::Normal, chord("d"), command) ==
             Keymap::Match::Complete);
    NX_CHECK(command == Command::DeleteLine);

    NX_CHECK(keymap.feed(EditingMode::Vim, EditorMode::Normal, chord("y"), command) ==
             Keymap::Match::Pending);
    NX_CHECK(keymap.feed(EditingMode::Vim, EditorMode::Normal, chord("y"), command) ==
             Keymap::Match::Complete);
    NX_CHECK(command == Command::YankLine);
}

NX_TEST_CASE(an_abandoned_prefix_does_not_swallow_the_next_key) {
    // "d" then "x": the prefix dies and "x" must still delete a character.
    Keymap keymap;
    Command command = Command::None;
    NX_CHECK(keymap.feed(EditingMode::Vim, EditorMode::Normal, chord("d"), command) ==
             Keymap::Match::Pending);
    NX_CHECK(keymap.feed(EditingMode::Vim, EditorMode::Normal, chord("x"), command) ==
             Keymap::Match::Complete);
    NX_CHECK(command == Command::DeleteChar);
}

NX_TEST_CASE(reset_pending_cancels_a_prefix) {
    Keymap keymap;
    Command command = Command::None;
    NX_CHECK(keymap.feed(EditingMode::Vim, EditorMode::Normal, chord("d"), command) ==
             Keymap::Match::Pending);
    keymap.reset_pending();
    // A second "d" now starts a fresh sequence rather than completing "dd".
    NX_CHECK(keymap.feed(EditingMode::Vim, EditorMode::Normal, chord("d"), command) ==
             Keymap::Match::Pending);
}

// --- rebinding (the future plugin surface) ----------------------------------

NX_TEST_CASE(bind_overrides_a_default) {
    Keymap keymap;
    keymap.bind(EditingMode::Vim, EditorMode::Normal, chord("Ctrl+S"), Command::Build);

    Command command = Command::None;
    NX_CHECK(keymap.feed(EditingMode::Vim, EditorMode::Normal, chord("Ctrl+S"), command) ==
             Keymap::Match::Complete);
    NX_CHECK(command == Command::Build);
}

NX_TEST_CASE(bind_by_name_targets_normal_and_insert) {
    Keymap keymap;
    // The shape the Lua API is expected to use.
    NX_CHECK(keymap.bind("normal", "Ctrl+B", "help"));
    NX_CHECK(keymap.bind("insert", "Ctrl+B", "help"));

    Command command = Command::None;
    NX_CHECK(keymap.feed(EditingMode::Vim, EditorMode::Normal, chord("Ctrl+B"), command) ==
             Keymap::Match::Complete);
    NX_CHECK(command == Command::Help);
    NX_CHECK(keymap.feed(EditingMode::Hybrid, EditorMode::Insert, chord("Ctrl+B"), command) ==
             Keymap::Match::Complete);
    NX_CHECK(command == Command::Help);
}

NX_TEST_CASE(bind_by_name_rejects_unknown_names) {
    Keymap keymap;
    NX_CHECK(!keymap.bind("normal", "Ctrl+B", "no_such_command"));
    NX_CHECK(!keymap.bind("normal", "NotAKey", "save"));
    NX_CHECK(!keymap.bind("nonsense", "Ctrl+B", "save"));
}

NX_TEST_CASE(unbind_removes_a_binding) {
    Keymap keymap;
    keymap.unbind(EditingMode::Vim, EditorMode::Normal, {chord("Ctrl+S")});
    Command command = Command::None;
    NX_CHECK(keymap.feed(EditingMode::Vim, EditorMode::Normal, chord("Ctrl+S"), command) ==
             Keymap::Match::None);
}

// --- command names ----------------------------------------------------------

NX_TEST_CASE(command_names_round_trip) {
    const Command all[] = {
        Command::Save,        Command::WriteOut,   Command::Quit,
        Command::Undo,        Command::Redo,       Command::CutLine,
        Command::Paste,       Command::Search,     Command::Replace,
        Command::GoToLine,    Command::DeleteLine, Command::YankLine,
    };
    for (const Command command : all) {
        const std::optional<Command> back = command_from_name(command_name(command));
        NX_CHECK(back.has_value());
        NX_CHECK(*back == command);
    }
    NX_CHECK(!command_from_name("definitely_not_a_command").has_value());
}

// --- modes and footers ------------------------------------------------------

NX_TEST_CASE(editing_mode_names_round_trip) {
    NX_CHECK(editing_mode_from_string("vim").value() == EditingMode::Vim);
    NX_CHECK(editing_mode_from_string("nano").value() == EditingMode::Nano);
    NX_CHECK(editing_mode_from_string("hybrid").value() == EditingMode::Hybrid);
    NX_CHECK(!editing_mode_from_string("emacs").has_value());

    NX_CHECK_EQ(std::string(editing_mode_name(EditingMode::Nano)), std::string("nano"));
}

NX_TEST_CASE(status_word_is_editor_for_nano) {
    NX_CHECK_EQ(std::string(editor_mode_name(EditingMode::Nano, EditorMode::Insert)),
                std::string("EDITOR"));
    NX_CHECK_EQ(std::string(editor_mode_name(EditingMode::Vim, EditorMode::Normal)),
                std::string("NORMAL"));
    NX_CHECK_EQ(std::string(editor_mode_name(EditingMode::Vim, EditorMode::Insert)),
                std::string("INSERT"));
    NX_CHECK_EQ(std::string(editor_mode_name(EditingMode::Hybrid, EditorMode::Command)),
                std::string("COMMAND"));
}

NX_TEST_CASE(footer_follows_the_mode_and_state) {
    NX_CHECK_EQ(Keymap::footer_hint(EditingMode::Vim, EditorMode::Insert),
                std::string("ESC Normal  ^S Save  ^Q Quit"));
    NX_CHECK_EQ(Keymap::footer_hint(EditingMode::Hybrid, EditorMode::Insert),
                std::string("ESC Normal  ^O Save  ^W Search  ^K Cut  ^U Paste"));
    NX_CHECK_EQ(Keymap::footer_hint(EditingMode::Vim, EditorMode::Normal),
                std::string("^P Commands  ^B Build  F4 Run  ^R Redo  : Command  ^Q Quit"));

    const std::string nano_footer =
        Keymap::footer_hint(EditingMode::Nano, EditorMode::Insert);
    for (const char* key : {"^G Help", "^O Save", "^W Search", "^K Cut", "^X Exit",
                            "^U Paste", "^\\ Replace", "^_ Go To"}) {
        NX_CHECK(nano_footer.find(key) != std::string::npos);
    }
}

NX_TEST_CASE(bindings_for_reports_the_live_state_only) {
    const Keymap keymap;
    const std::vector<std::pair<std::string, std::string>> nano_keys =
        keymap.bindings_for(EditingMode::Nano, EditorMode::Insert);
    NX_CHECK(!nano_keys.empty());

    bool has_write_out = false;
    for (const std::pair<std::string, std::string>& binding : nano_keys) {
        // Nano never contains a Vim NORMAL-only binding.
        NX_CHECK(binding.second != std::string("vim_insert"));
        if (binding.second == std::string("write_out")) {
            has_write_out = true;
        }
    }
    NX_CHECK(has_write_out);
}
