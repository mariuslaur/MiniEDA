#pragma once
#include <stdexcept>
#include <string>
#include <vector>

namespace MiniEDA {

// Custom exception for precise tracking of compiler/parser syntax issues
class SyntaxException : public std::runtime_error {
public:
    SyntaxException(const std::string& message, size_t line, size_t column)
        : std::runtime_error(message + " at line " + std::to_string(line) + ", column " + std::to_string(column)),
          m_line(line), m_column(column), m_rawMessage(message) {}

    size_t line() const { return m_line; }
    size_t column() const { return m_column; }
    std::string rawMessage() const { return m_rawMessage; }

private:
    size_t m_line;
    size_t m_column;
    std::string m_rawMessage;
};

// Token value with its source coordinates (1-based)
struct Token {
    std::string value;
    size_t line;
    size_t column;
};

// Splits Verilog source into tokens. Whitespace, '\r', '//' and '/* */' comments are dropped;
// single-character delimiters (see isDelimiter) become their own tokens.
std::vector<Token> tokenize(const std::string& code);

// True for single-character delimiter tokens: ; , ( ) [ ] : = & | ^ ~ !
bool isDelimiter(const std::string& value);

} // namespace MiniEDA
