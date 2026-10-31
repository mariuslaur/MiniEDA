#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <utility>

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

    // Registers a net name into the given port list (no duplicates) and initializes its state to Z
    void addPort(std::vector<std::string>& list, const std::string& name);

    // Vector support: "[msb:lsb] name" declarations are expanded into scalar bit nets "name[k]"
    std::unordered_map<std::string, std::pair<int, int>> m_vectorRanges;
    void declareNets(std::vector<std::string>& list, const std::string& name, bool isVector, int msb, int lsb);
    bool parseRange(const std::vector<Token>& tokens, size_t& i, int& msb, int& lsb) const;
    std::string parseNetRef(const std::vector<Token>& tokens, size_t& i) const;

    // Continuous assignment lowering: boolean expressions become gates driving temporary nets
    size_t m_assignCounter = 0;
    std::string parseOrExpr(const std::vector<Token>& tokens, size_t& i, size_t line);
    std::string parseXorExpr(const std::vector<Token>& tokens, size_t& i, size_t line);
    std::string parseAndExpr(const std::vector<Token>& tokens, size_t& i, size_t line);
    std::string parseUnaryExpr(const std::vector<Token>& tokens, size_t& i, size_t line);
    std::string emitAssignGate(GateType type, const std::vector<std::string>& inputs, size_t line);

    // Updated to extract structural tokens with spatial metadata
    void tokenize(const std::string& code, std::vector<Token>& tokens) const;
};

} // namespace MiniEDA