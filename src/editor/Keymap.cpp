#include "nanox/editor/Keymap.h"

#include <algorithm>
#include <cctype>

namespace nanox::editor {
namespace {

KeyChord make_char(char c) {
    KeyChord chord;
    chord.kind = Key::Kind::Char;
    chord.ch = c;
    return chord;
}

KeyChord make_ctrl(char c) {
    KeyChord chord = make_char(c);
    chord.ctrl = true;
    return chord;
}

KeyChord make_alt(char c) {
    KeyChord chord = make_char(c);
    chord.alt = true;
    return chord;
}

KeyChord make_kind(Key::Kind kind) {
    KeyChord chord;
    chord.kind = kind;
    return chord;
}

KeyChord make_fn(unsigned n) {
    KeyChord chord;
    chord.kind = Key::Kind::Function;
    chord.param = n;
    return chord;
}

char lower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

char upper(char c) {
    return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

// Names accepted by chord_from_string for keys that have no printable form.
struct NamedKey {
    const char* name;
    Key::Kind kind;
};

constexpr NamedKey kNamedKeys[] = {
    {"Enter", Key::Kind::Enter},         {"Return", Key::Kind::Enter},
    {"Escape", Key::Kind::Escape},       {"Esc", Key::Kind::Escape},
    {"Tab", Key::Kind::Tab},             {"Backspace", Key::Kind::Backspace},
    {"BS", Key::Kind::Backspace},        {"Delete", Key::Kind::Delete},
    {"Del", Key::Kind::Delete},          {"Home", Key::Kind::Home},
    {"End", Key::Kind::End},             {"Up", Key::Kind::ArrowUp},
    {"Down", Key::Kind::ArrowDown},      {"Left", Key::Kind::ArrowLeft},
    {"Right", Key::Kind::ArrowRight},    {"PageUp", Key::Kind::PageUp},
    {"PageDown", Key::Kind::PageDown},   {"Space", Key::Kind::Char},
};

const char* kind_to_name(Key::Kind kind) {
    switch (kind) {
        case Key::Kind::Enter: return "Enter";
        case Key::Kind::Escape: return "Escape";
        case Key::Kind::Tab: return "Tab";
        case Key::Kind::Backspace: return "Backspace";
        case Key::Kind::Delete: return "Delete";
        case Key::Kind::Home: return "Home";
        case Key::Kind::End: return "End";
        case Key::Kind::ArrowUp: return "Up";
        case Key::Kind::ArrowDown: return "Down";
        case Key::Kind::ArrowLeft: return "Left";
        case Key::Kind::ArrowRight: return "Right";
        case Key::Kind::PageUp: return "PageUp";
        case Key::Kind::PageDown: return "PageDown";
        default: return nullptr;
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// EditingMode / EditorMode
// ---------------------------------------------------------------------------

std::optional<EditingMode> editing_mode_from_string(std::string_view text) {
    if (text == "vim") return EditingMode::Vim;
    if (text == "nano") return EditingMode::Nano;
    if (text == "hybrid") return EditingMode::Hybrid;
    return std::nullopt;
}

std::string_view editing_mode_name(EditingMode mode) {
    switch (mode) {
        case EditingMode::Vim: return "vim";
        case EditingMode::Nano: return "nano";
        case EditingMode::Hybrid: return "hybrid";
    }
    return "vim";
}

std::string_view editor_mode_name(EditingMode editing, EditorMode mode) {
    if (editing == EditingMode::Nano) {
        return "EDITOR";  // nano has no modes; the status line says EDITOR
    }
    switch (mode) {
        case EditorMode::Normal: return "NORMAL";
        case EditorMode::Insert: return "INSERT";
        case EditorMode::Command: return "COMMAND";
    }
    return "NORMAL";
}

// ---------------------------------------------------------------------------
// KeyChord
// ---------------------------------------------------------------------------

KeyChord KeyChord::from_key(const Key& key) {
    KeyChord chord;
    chord.kind = key.kind;
    chord.ch = key.ch;
    chord.param = key.param;

    // Fold the modifier kinds onto Char + a flag, lowercasing letters so that
    // Ctrl+S from the terminal always matches the "Ctrl+S" notation.
    if (key.kind == Key::Kind::Ctrl) {
        chord.kind = Key::Kind::Char;
        chord.ctrl = true;
        chord.ch = lower(key.ch);
    } else if (key.kind == Key::Kind::Alt) {
        chord.kind = Key::Kind::Char;
        chord.alt = true;
        chord.ch = lower(key.ch);
    }
    return chord;
}

std::string KeyChord::to_string() const {
    if (kind == Key::Kind::Function) {
        return "F" + std::to_string(param);
    }

    std::string body;
    if (kind == Key::Kind::Char) {
        body = std::string(1, ch);
    } else {
        const char* named = kind_to_name(kind);
        if (named == nullptr) {
            return "?";
        }
        body = named;
    }

    // Modifiers are only spelled out for combinations; a bare 'S' stays "S".
    if (ctrl) {
        return "Ctrl+" + std::string(1, upper(ch));
    }
    if (alt) {
        return "Alt+" + std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
    if (body == " ") {
        return "Space";
    }
    return body;
}

bool KeyChord::operator==(const KeyChord& other) const {
    return kind == other.kind && ch == other.ch && ctrl == other.ctrl &&
           alt == other.alt && param == other.param;
}

bool KeyChord::operator<(const KeyChord& other) const {
    if (kind != other.kind) return kind < other.kind;
    if (ch != other.ch) return ch < other.ch;
    if (ctrl != other.ctrl) return ctrl < other.ctrl;
    if (alt != other.alt) return alt < other.alt;
    return param < other.param;
}

std::optional<KeyChord> chord_from_string(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    bool ctrl = false;
    bool alt = false;

    // Accept both "Ctrl+X" / "Alt+X" and the ^X shorthand used in the footer.
    if (text.size() >= 2 && text[0] == '^') {
        ctrl = true;
        text.remove_prefix(1);
    }
    while (true) {
        if (text.size() > 5 && (text.substr(0, 5) == "Ctrl+" || text.substr(0, 5) == "ctrl+")) {
            ctrl = true;
            text.remove_prefix(5);
        } else if (text.size() > 4 && (text.substr(0, 4) == "Alt+" || text.substr(0, 4) == "alt+")) {
            alt = true;
            text.remove_prefix(4);
        } else if (text.size() > 2 && (text.substr(0, 2) == "C-" || text.substr(0, 2) == "M-")) {
            if (text[0] == 'C') {
                ctrl = true;
            } else {
                alt = true;
            }
            text.remove_prefix(2);
        } else {
            break;
        }
    }

    if (text.empty()) {
        return std::nullopt;
    }

    // F1..F12
    if ((text[0] == 'F' || text[0] == 'f') && text.size() <= 3 && text.size() >= 2) {
        bool digits = true;
        for (std::size_t i = 1; i < text.size(); ++i) {
            if (text[i] < '0' || text[i] > '9') {
                digits = false;
                break;
            }
        }
        if (digits) {
            const int n = std::stoi(std::string(text.substr(1)));
            if (n >= 1 && n <= 12) {
                return make_fn(static_cast<unsigned>(n));
            }
            return std::nullopt;
        }
    }

    for (const NamedKey& named : kNamedKeys) {
        if (text == named.name) {
            KeyChord chord = make_kind(named.kind);
            if (named.kind == Key::Kind::Char) {
                chord.ch = ' ';
            }
            // A named key with a modifier makes no sense here except Space.
            if (chord.kind == Key::Kind::Char) {
                chord.ctrl = ctrl;
                chord.alt = alt;
            } else if (ctrl || alt) {
                return std::nullopt;
            }
            return chord;
        }
    }

    if (text.size() == 1) {
        KeyChord chord = make_char(text[0]);
        chord.ctrl = ctrl;
        chord.alt = alt;
        // "Ctrl+S" and "Ctrl+s" must denote the same key.
        if (ctrl || alt) {
            chord.ch = lower(chord.ch);
        }
        return chord;
    }

    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Keymap
// ---------------------------------------------------------------------------

Keymap::Keymap() {
    using M = EditingMode;
    using E = EditorMode;
    const KeyChord esc = make_kind(Key::Kind::Escape);

    // --- Vim: NORMAL --------------------------------------------------------
    const KeyBinding vim_normal[] = {
        {M::Vim, E::Normal, {make_char('i')}, Command::VimInsert},
        {M::Vim, E::Normal, {make_char('a')}, Command::VimAppend},
        {M::Vim, E::Normal, {make_char('I')}, Command::VimInsertLineStart},
        {M::Vim, E::Normal, {make_char('A')}, Command::VimAppendLineEnd},
        {M::Vim, E::Normal, {make_char('o')}, Command::VimOpenBelow},
        {M::Vim, E::Normal, {make_char('O')}, Command::VimOpenAbove},
        {M::Vim, E::Normal, {make_char('h')}, Command::MoveLeft},
        {M::Vim, E::Normal, {make_char('j')}, Command::MoveDown},
        {M::Vim, E::Normal, {make_char('k')}, Command::MoveUp},
        {M::Vim, E::Normal, {make_char('l')}, Command::MoveRight},
        {M::Vim, E::Normal, {make_char('0')}, Command::LineStart},
        {M::Vim, E::Normal, {make_char('$')}, Command::LineEnd},
        {M::Vim, E::Normal, {make_char('x')}, Command::DeleteChar},
        {M::Vim, E::Normal, {make_char('D')}, Command::DeleteToEnd},
        {M::Vim, E::Normal, {make_char('p')}, Command::PasteAfter},
        {M::Vim, E::Normal, {make_char('P')}, Command::PasteBefore},
        {M::Vim, E::Normal, {make_char('u')}, Command::Undo},
        {M::Vim, E::Normal, {make_char('d'), make_char('d')}, Command::DeleteLine},
        {M::Vim, E::Normal, {make_char('y'), make_char('y')}, Command::YankLine},
        {M::Vim, E::Normal, {make_char(':')}, Command::EnterCommandMode},
        {M::Vim, E::Normal, {make_ctrl('p')}, Command::EnterCommandMode},
        {M::Vim, E::Normal, {make_ctrl('b')}, Command::Build},
        {M::Vim, E::Normal, {make_ctrl('r')}, Command::Redo},
        {M::Vim, E::Normal, {make_ctrl('s')}, Command::Save},
        {M::Vim, E::Normal, {make_ctrl('q')}, Command::Quit},
        {M::Vim, E::Normal, {make_ctrl('c')}, Command::Quit},
        {M::Vim, E::Normal, {make_ctrl('w')}, Command::CloseTab},
        {M::Vim, E::Normal, {make_ctrl('t')}, Command::NextTab},
        {M::Vim, E::Normal, {make_ctrl('x')}, Command::CloseTab},
        {M::Vim, E::Normal, {esc}, Command::EnterNormal},
        {M::Vim, E::Normal, {make_kind(Key::Kind::ArrowLeft)}, Command::MoveLeft},
        {M::Vim, E::Normal, {make_kind(Key::Kind::ArrowDown)}, Command::MoveDown},
        {M::Vim, E::Normal, {make_kind(Key::Kind::ArrowUp)}, Command::MoveUp},
        {M::Vim, E::Normal, {make_kind(Key::Kind::ArrowRight)}, Command::MoveRight},
        {M::Vim, E::Normal, {make_kind(Key::Kind::Home)}, Command::LineStart},
        {M::Vim, E::Normal, {make_kind(Key::Kind::End)}, Command::LineEnd},
    };

    // --- Vim: INSERT (the footer lists only ESC / ^S / ^Q) -------------------
    const KeyBinding vim_insert[] = {
        {M::Vim, E::Insert, {esc}, Command::EnterNormal},
        {M::Vim, E::Insert, {make_ctrl('s')}, Command::Save},
        {M::Vim, E::Insert, {make_ctrl('q')}, Command::Quit},
        {M::Vim, E::Insert, {make_ctrl('c')}, Command::Quit},
        {M::Vim, E::Insert, {make_ctrl('x')}, Command::CloseTab},
        {M::Vim, E::Insert, {make_ctrl('t')}, Command::NextTab},
    };

    // --- Nano: a single, always-editing state --------------------------------
    const KeyBinding nano[] = {
        {M::Nano, E::Insert, {make_ctrl('o')}, Command::WriteOut},
        {M::Nano, E::Insert, {make_ctrl('x')}, Command::CloseTab},
        {M::Nano, E::Insert, {make_ctrl('w')}, Command::Search},
        {M::Nano, E::Insert, {make_ctrl('\\')}, Command::Replace},
        {M::Nano, E::Insert, {make_ctrl('k')}, Command::CutLine},
        {M::Nano, E::Insert, {make_ctrl('u')}, Command::Paste},
        {M::Nano, E::Insert, {make_ctrl('g')}, Command::Help},
        {M::Nano, E::Insert, {make_ctrl('_')}, Command::GoToLine},
        {M::Nano, E::Insert, {make_alt('u')}, Command::Undo},
        {M::Nano, E::Insert, {make_alt('e')}, Command::Redo},
        {M::Nano, E::Insert, {make_alt('6')}, Command::CopyLine},
        {M::Nano, E::Insert, {make_ctrl('s')}, Command::Save},
        {M::Nano, E::Insert, {make_ctrl('q')}, Command::Quit},
        {M::Nano, E::Insert, {make_ctrl('c')}, Command::Quit},
        {M::Nano, E::Insert, {make_ctrl('t')}, Command::NextTab},
        // Nano leaves ^R free, so Run keeps the footer binding the spec shows.
        {M::Nano, E::Insert, {make_ctrl('r')}, Command::Run},
        {M::Nano, E::Insert, {make_ctrl('b')}, Command::Build},
        {M::Nano, E::Insert, {make_ctrl('p')}, Command::EnterCommandMode},
    };

    // --- Hybrid: Vim's NORMAL, nano's INSERT ---------------------------------
    std::vector<KeyBinding> hybrid_normal;
    for (const KeyBinding& binding : vim_normal) {
        KeyBinding copy = binding;
        copy.editing = M::Hybrid;
        hybrid_normal.push_back(std::move(copy));
    }
    const KeyBinding hybrid_insert[] = {
        {M::Hybrid, E::Insert, {esc}, Command::EnterNormal},
        {M::Hybrid, E::Insert, {make_ctrl('o')}, Command::WriteOut},
        {M::Hybrid, E::Insert, {make_ctrl('w')}, Command::Search},
        {M::Hybrid, E::Insert, {make_ctrl('k')}, Command::CutLine},
        {M::Hybrid, E::Insert, {make_ctrl('u')}, Command::Paste},
        {M::Hybrid, E::Insert, {make_ctrl('g')}, Command::Help},
        {M::Hybrid, E::Insert, {make_ctrl('s')}, Command::Save},
        {M::Hybrid, E::Insert, {make_ctrl('q')}, Command::Quit},
        {M::Hybrid, E::Insert, {make_ctrl('c')}, Command::Quit},
        {M::Hybrid, E::Insert, {make_ctrl('x')}, Command::CloseTab},
        {M::Hybrid, E::Insert, {make_ctrl('t')}, Command::NextTab},
    };

    // EditorMode::Command intentionally has no bindings: while a prompt is open
    // it consumes every key itself (see handle_prompt_key), so the state exists
    // only to label the status bar. The empty state is what the footer and help
    // overlay report if they are ever asked about it.

    for (const KeyBinding& binding : vim_normal) bindings_.push_back(binding);
    for (const KeyBinding& binding : vim_insert) bindings_.push_back(binding);
    for (const KeyBinding& binding : nano) bindings_.push_back(binding);
    for (const KeyBinding& binding : hybrid_normal) bindings_.push_back(binding);
    for (const KeyBinding& binding : hybrid_insert) bindings_.push_back(binding);
}

void Keymap::bind(EditingMode editing, EditorMode editor, KeyChord chord, Command command) {
    bind_sequence(editing, editor, {chord}, command);
}

void Keymap::bind_sequence(EditingMode editing, EditorMode editor,
                           std::vector<KeyChord> chords, Command command) {
    if (chords.empty()) {
        return;
    }
    // Rebinding replaces: a key resolves to exactly one command per state.
    for (KeyBinding& binding : bindings_) {
        if (binding.editing == editing && binding.editor == editor &&
            binding.chords == chords) {
            binding.command = command;
            return;
        }
    }
    KeyBinding binding;
    binding.editing = editing;
    binding.editor = editor;
    binding.chords = std::move(chords);
    binding.command = command;
    bindings_.push_back(std::move(binding));
}

bool Keymap::bind(std::string_view editor_mode, std::string_view chord_text,
                  std::string_view command_text) {
    const std::optional<KeyChord> chord = chord_from_string(chord_text);
    const std::optional<Command> command = command_from_name(command_text);
    if (!chord.has_value() || !command.has_value()) {
        return false;
    }

    // "normal"/"insert" address the vim-style states in both Vim and Hybrid;
    // "nano" addresses nano's single always-editing state. Mirrors how the
    // future `nanox.keymap("normal", "Ctrl+S", "save")` call is expected to read.
    if (editor_mode == "normal") {
        bind(EditingMode::Vim, EditorMode::Normal, *chord, *command);
        bind(EditingMode::Hybrid, EditorMode::Normal, *chord, *command);
        return true;
    }
    if (editor_mode == "insert") {
        bind(EditingMode::Vim, EditorMode::Insert, *chord, *command);
        bind(EditingMode::Hybrid, EditorMode::Insert, *chord, *command);
        return true;
    }
    if (editor_mode == "nano") {
        bind(EditingMode::Nano, EditorMode::Insert, *chord, *command);
        return true;
    }
    return false;
}

void Keymap::unbind(EditingMode editing, EditorMode editor, const std::vector<KeyChord>& chords) {
    bindings_.erase(
        std::remove_if(bindings_.begin(), bindings_.end(),
                       [&](const KeyBinding& binding) {
                           return binding.editing == editing && binding.editor == editor &&
                                  binding.chords == chords;
                       }),
        bindings_.end());
}

Keymap::Match Keymap::feed(EditingMode editing, EditorMode editor, KeyChord chord, Command& out) {
    // A sequence that has not completed yet is retried as a fresh start, so
    // "d" then "x" still deletes a character instead of silently swallowing it.
    for (int attempt = 0; attempt < 2; ++attempt) {
        pending_.push_back(chord);
        bool longer = false;

        for (const KeyBinding& binding : bindings_) {
            if (binding.editing != editing || binding.editor != editor) {
                continue;
            }
            if (binding.chords.size() < pending_.size()) {
                continue;
            }
            if (!std::equal(pending_.begin(), pending_.end(), binding.chords.begin())) {
                continue;
            }
            if (binding.chords.size() == pending_.size()) {
                // Exact match wins over a longer binding that shares the prefix.
                out = binding.command;
                pending_.clear();
                return Match::Complete;
            }
            longer = true;
        }

        if (longer) {
            return Match::Pending;
        }

        const bool was_multi = pending_.size() > 1;
        pending_.clear();
        if (!was_multi) {
            return Match::None;
        }
        // else: retry this chord on its own
    }
    return Match::None;
}

void Keymap::reset_pending() {
    pending_.clear();
}

std::vector<std::pair<std::string, std::string>> Keymap::bindings_for(
    EditingMode editing, EditorMode editor) const {
    std::vector<std::pair<std::string, std::string>> out;
    for (const KeyBinding& binding : bindings_) {
        if (binding.editing != editing || binding.editor != editor ||
            binding.command == Command::None) {
            continue;
        }
        std::string keys;
        for (const KeyChord& chord : binding.chords) {
            keys += chord.to_string();
        }
        out.emplace_back(keys, std::string(command_name(binding.command)));
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::string Keymap::footer_hint(EditingMode editing, EditorMode editor) {
    if (editing == EditingMode::Nano) {
        // nano has one state, so its two spec rows are shown as one footer row.
        return "^G Help  ^O Save  ^W Search  ^K Cut   ^X Exit  ^U Paste  ^\\ Replace  ^_ Go To";
    }
    switch (editor) {
        case EditorMode::Normal:
            // ^R is Redo here (Vim semantics), so Run is shown on F4.
            return "^P Commands  ^B Build  F4 Run  ^R Redo  : Command  ^Q Quit";
        case EditorMode::Insert:
            if (editing == EditingMode::Hybrid) {
                return "ESC Normal  ^O Save  ^W Search  ^K Cut  ^U Paste";
            }
            return "ESC Normal  ^S Save  ^Q Quit";
        case EditorMode::Command:
            return "Enter Run  ESC Cancel";
    }
    return "";
}

}  // namespace nanox::editor
