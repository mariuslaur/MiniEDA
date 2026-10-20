#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

namespace MiniEDA {

enum class LogicState {
    LogicLow  = 0,
    LogicHigh = 1,
    HighImpedance = 2
};

enum class GateType {
    AND, OR, XOR, NAND, NOR, XNOR, BUF, NOT
};

struct Gate {
    std::string name;
    GateType type;
    std::string outputWire;
    std::vector<std::string> inputWires;
};

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

// Internal structure to pack token value with its coordinates
struct Token {
    std::string value;
    size_t line;
    size_t column;
};

class Parser {
public:
    Parser() = default;
    ~Parser() = default;

    // Parses Verilog netlists. Throws SyntaxException on failure.
    bool parseNetlist(const std::string& verilogCode);
    void evaluateCircuit();

    void setInputState(const std::string& inputName, LogicState state);
    LogicState getWireState(const std::string& wireName) const;

    const std::vector<std::string>& getInputs() const { return m_inputs; }
    const std::vector<std::string>& getOutputs() const { return m_outputs; }
    const std::vector<Gate>& getGates() const { return m_gates; }

private:
    std::vector<std::string> m_inputs;
    std::vector<std::string> m_outputs;
    std::vector<std::string> m_wires;
    std::vector<Gate> m_gates;
    std::unordered_map<std::string, LogicState> m_wireStates;

    LogicState evaluateGate(GateType type, const std::vector<LogicState>& inputs) const;
    GateType stringToGateType(const std::string& gateStr, size_t line, size_t col) const;
    
    // Updated to extract structural tokens with spatial metadata
    void tokenize(const std::string& code, std::vector<Token>& tokens) const;
};

} // namespace MiniEDA