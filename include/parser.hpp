#pragma once
#include "lexer.hpp"
#include "netlist.hpp"
#include <algorithm>
#include <unordered_map>
#include <utility>

namespace MiniEDA {

// Parses one Verilog module (structural gates, 1-bit dataflow assigns, vectors) into a Netlist.
// Throws SyntaxException with line/column on failure.
class Parser {
public:
    Netlist parse(const std::string& verilogCode);

private:
    // State of the parse in progress
    std::vector<Token> m_tokens;
    size_t m_pos = 0;
    Netlist m_netlist;
    std::unordered_map<std::string, std::pair<int, int>> m_vectorRanges; // vector name -> [msb, lsb]
    size_t m_assignCounter = 0;

    // Statements; each one leaves m_pos on its terminating ';'
    void parseModuleHeader();
    void parseDeclaration();
    void parseGateInstance();
    void parseAssign();

    // Vector support: "[msb:lsb] name" declarations are expanded into scalar bit nets "name[k]"
    bool parseRange(int& msb, int& lsb);
    std::string parseNetRef();
    void declareNets(std::vector<std::string>& list, const std::string& name, bool isVector, int msb, int lsb);

    // Continuous assignment lowering: boolean expressions become gates driving temporary nets
    std::string parseOrExpr(size_t line);
    std::string parseXorExpr(size_t line);
    std::string parseAndExpr(size_t line);
    std::string parseUnaryExpr(size_t line);
    std::string emitAssignGate(GateType type, const std::vector<std::string>& inputs, size_t line);

    // Token helpers
    bool atEnd() const { return m_pos >= m_tokens.size(); }
    bool peekIs(const char* value) const { return !atEnd() && m_tokens[m_pos].value == value; }
    // Token at the cursor, or the last token once the input is exhausted (for error coordinates)
    const Token& current() const { return m_tokens[std::min(m_pos, m_tokens.size() - 1)]; }
    // True when the next two tokens are `first` and `second` written without a gap (e.g. "~^", "&&")
    bool peekAdjacentPair(const char* first, const char* second) const;
    [[noreturn]] void fail(const std::string& message, const Token& at) const;
};

} // namespace MiniEDA
