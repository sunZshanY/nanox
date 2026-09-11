#include "nanox/editor/CommandParser.h"

#include "test_framework.hpp"

#include <string>

using namespace nanox::editor;

NX_TEST_CASE(parse_write) {
    const ParsedCommand parsed = parse_ex_command(":w");
    NX_CHECK(parsed.ok);
    NX_CHECK(parsed.command == Command::Save);
    NX_CHECK(parsed.arg.empty());
    NX_CHECK(!parsed.force);
}

NX_TEST_CASE(parse_write_with_a_path_is_a_save_as) {
    const ParsedCommand parsed = parse_ex_command(":w build/main.nx");
    NX_CHECK(parsed.ok);
    NX_CHECK(parsed.command == Command::Save);
    NX_CHECK_EQ(parsed.arg, std::string("build/main.nx"));
}

NX_TEST_CASE(parse_quit_and_force_quit) {
    const ParsedCommand quit = parse_ex_command(":q");
    NX_CHECK(quit.ok);
    NX_CHECK(quit.command == Command::Quit);
    NX_CHECK(!quit.force);

    const ParsedCommand force = parse_ex_command(":q!");
    NX_CHECK(force.ok);
    NX_CHECK(force.command == Command::ForceQuit);
    NX_CHECK(force.force);
}

NX_TEST_CASE(parse_write_and_quit) {
    const ParsedCommand wq = parse_ex_command(":wq");
    NX_CHECK(wq.ok);
    NX_CHECK(wq.command == Command::SaveAndQuit);

    // :x is Vim's alias for the same thing.
    const ParsedCommand x = parse_ex_command(":x");
    NX_CHECK(x.ok);
    NX_CHECK(x.command == Command::SaveAndQuit);

    const ParsedCommand wq_bang = parse_ex_command(":wq!");
    NX_CHECK(wq_bang.ok);
    NX_CHECK(wq_bang.command == Command::SaveAndQuit);
    NX_CHECK(wq_bang.force);
}

NX_TEST_CASE(parse_set_mode) {
    const ParsedCommand nano = parse_ex_command(":set mode nano");
    NX_CHECK(nano.ok);
    NX_CHECK(nano.command == Command::SetEditingMode);
    NX_CHECK_EQ(nano.arg, std::string("nano"));

    const ParsedCommand hybrid = parse_ex_command(":set mode hybrid");
    NX_CHECK(hybrid.ok);
    NX_CHECK_EQ(hybrid.arg, std::string("hybrid"));
}

NX_TEST_CASE(parse_set_mode_rejects_an_unknown_mode) {
    const ParsedCommand parsed = parse_ex_command(":set mode emacs");
    NX_CHECK(!parsed.ok);
    NX_CHECK(parsed.command == Command::None);
}

NX_TEST_CASE(parse_is_lenient_about_whitespace_and_the_leading_colon) {
    NX_CHECK(parse_ex_command("w").command == Command::Save);
    NX_CHECK(parse_ex_command("  :w  ").command == Command::Save);
    NX_CHECK(parse_ex_command(":  w").command == Command::Save);
    NX_CHECK(parse_ex_command(":q ").command == Command::Quit);
    NX_CHECK(parse_ex_command(":set   mode   vim").ok);
}

NX_TEST_CASE(parse_rejects_unknown_commands) {
    NX_CHECK(!parse_ex_command(":frobnicate").ok);
    NX_CHECK(!parse_ex_command("").ok);
    NX_CHECK(!parse_ex_command(":").ok);
    NX_CHECK(!parse_ex_command(":set").ok);
    NX_CHECK(!parse_ex_command(":set mode").ok);
}
