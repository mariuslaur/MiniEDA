#include "lexer.hpp"
#include <cctype>

namespace MiniEDA {

namespace {
const std::string kDelimiters = ";,()[]:=&|^~!";
}

bool isDelimiter(const std::string& value) {
    return value.size() == 1 && kDelimiters.find(value[0]) != std::string::npos;
}

std::vector<Token> tokenize(const std::string& code) {
    std::vector<Token> tokens;
    std::string current;
    size_t currentLine = 1;
    size_t currentColumn = 1;
    size_t tokenStartColumn = 1;

    auto flush = [&]() {
        if (!current.empty()) {
            tokens.push_back({current, currentLine, tokenStartColumn});
            current.clear();
        }
    };

    for (size_t i = 0; i < code.length(); ++i) {
        char ch = code[i];

        // Skip Carriage Return '\r' characters to prevent Windows/Linux line-ending corruption
        if (ch == '\r') {
            currentColumn++;
            continue;
        }

        // Skip '//' line comments and '/* */' block comments, keeping line/column tracking intact
        if (ch == '/' && i + 1 < code.length() && (code[i + 1] == '/' || code[i + 1] == '*')) {
            flush();
            bool blockComment = (code[i + 1] == '*');
            i += 2;
            currentColumn += 2;
            while (i < code.length()) {
                if (!blockComment && code[i] == '\n') {
                    i--; // Let the main loop consume the newline and advance the line counter
                    break;
                }
                if (blockComment && code[i] == '*' && i + 1 < code.length() && code[i + 1] == '/') {
                    i++;
                    currentColumn += 2;
                    break;
                }
                if (code[i] == '\n') {
                    currentLine++;
                    currentColumn = 1;
                } else {
                    currentColumn++;
                }
                i++;
            }
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(ch))) {
            flush();
            if (ch == '\n') {
                currentLine++;
                currentColumn = 1;
            } else {
                currentColumn++;
            }
        } else if (kDelimiters.find(ch) != std::string::npos) {
            flush();
            // Add the special single character delimiter with its direct position
            tokens.push_back({std::string(1, ch), currentLine, currentColumn});
            currentColumn++;
        } else {
            if (current.empty()) {
                tokenStartColumn = currentColumn;
            }
            current += ch;
            currentColumn++;
        }
    }
    flush();
    return tokens;
}

} // namespace MiniEDA
