#include "nanox/editor/CommandParser.h"

#include "nanox/editor/Keymap.h"

namespace nanox::editor {
namespace {

constexpr std::string_view kWhitespace = " \t\r";

std::string_view trim(std::string_view text) {
    const std::size_t begin = text.find_first_not_of(kWhitespace);
    if (begin == std::string_view::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(kWhitespace);
    return text.substr(begin, end - begin + 1);
}

// Splits on the first run of whitespace: "mode nano" -> {"mode", "nano"}.
void split_head(std::string_view text, std::string_view& head, std::string_view& rest) {
    const std::size_t split = text.find_first_of(kWhitespace);
    if (split == std::string_view::npos) {
        head = text;
        rest = {};
        return;
    }
    head = text.substr(0, split);
    rest = trim(text.substr(split));
}

}  // namespace

ParsedCommand parse_ex_command(std::string_view line) {
    ParsedCommand result;

    std::string_view body = trim(line);
    if (!body.empty() && body.front() == ':') {
        body = trim(body.substr(1));
    }
    if (body.empty()) {
        return result;
    }

    std::string_view head;
    std::string_view rest;
    split_head(body, head, rest);

    // A trailing '!' applies to the whole command (:q!, :wq!).
    if (!head.empty() && head.back() == '!') {
        result.force = true;
        head.remove_suffix(1);
    }

    if (head == "w" || head == "write") {
        result.command = Command::Save;
        result.arg = std::string(rest);
        result.ok = true;
        return result;
    }
    if (head == "q" || head == "quit") {
        result.command = result.force ? Command::ForceQuit : Command::Quit;
        result.ok = true;
        return result;
    }
    // :x is Vim's "write if changed, then quit"; it maps onto the same action.
    if (head == "wq" || head == "x" || head == "xit") {
        result.command = Command::SaveAndQuit;
        result.ok = true;
        return result;
    }
    if (head == "set") {
        std::string_view key;
        std::string_view value;
        split_head(rest, key, value);
        if (key == "mode" && !value.empty()) {
            result.arg = std::string(value);
            // The name is validated here so an unknown mode is reported as a
            // bad command rather than switching to nothing.
            result.ok = editing_mode_from_string(value).has_value();
            if (result.ok) {
                result.command = Command::SetEditingMode;
            }
        }
        return result;
    }

    return result;
}

}  // namespace nanox::editor
