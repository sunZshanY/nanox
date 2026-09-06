// A tiny command-line tool that lexes a NanoX source file and dumps every
// token (kind, lexeme, location) to stdout. Diagnostics go to stderr.
//
//   nanox-lex <file.nx>
//
// This is useful for manually inspecting the lexer while later phases are
// still under construction.

#include "nanox/Diagnostic.h"
#include "nanox/Lexer.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: lexer_dump <file.nx>\n";
        return 2;
    }

    const std::string filename = argv[1];

    std::ifstream in(filename);
    if (!in) {
        std::cerr << "error: cannot open file: " << filename << '\n';
        return 1;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string source = buffer.str();

    nanox::DiagnosticEngine diagnostics(filename);
    nanox::Lexer lexer(source, filename, diagnostics);

    while (true) {
        const nanox::Token token = lexer.next();
        std::cout << token.location << '\t' << token.kind;
        if (!token.lexeme.empty()) {
            std::cout << "\t'" << token.lexeme << "'";
        }
        std::cout << '\n';
        if (token.is_eof()) {
            break;
        }
    }

    if (diagnostics.has_errors()) {
        diagnostics.print_all(std::cerr);
        return 1;
    }
    return 0;
}
