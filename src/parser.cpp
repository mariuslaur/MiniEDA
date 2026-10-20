#include "parser.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace MiniEDA {

void Parser::tokenize(const std::string& code, std::vector<Token>& tokens) const {
    std::string current;
    size_t currentLine = 1;
    size_t currentColumn = 1;
    size_t tokenStartColumn = 1;

    for (size_t i = 0; i < code.length(); ++i) {
        char ch = code[i];

        // REPAIR: Explicitly skip Carriage Return '\r' characters to prevent Windows/Linux line-ending corruption
        if (ch == '\r') {
            currentColumn++;
            continue;
        }

        if (std::isspace(ch)) {
            if (!current.empty()) {
                tokens.push_back({current, currentLine, tokenStartColumn});
                current.clear();
            }
            if (ch == '\n') {
                currentLine++;
                currentColumn = 1;
            } else {
                currentColumn++;
            }
        } else if (ch == ';' || ch == ',' || ch == '(' || ch == ')') {
            if (!current.empty()) {
                tokens.push_back({current, currentLine, tokenStartColumn});
                current.clear();
            }
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
    if (!current.empty()) {
        tokens.push_back({current, currentLine, tokenStartColumn});
    }
}

GateType Parser::stringToGateType(const std::string& gateStr, size_t line, size_t col) const {
    if (gateStr == "and")  return GateType::AND;
    if (gateStr == "or")   return GateType::OR;
    if (gateStr == "xor")  return GateType::XOR;
    if (gateStr == "nand") return GateType::NAND;
    if (gateStr == "nor")  return GateType::NOR;
    if (gateStr == "xnor") return GateType::XNOR;
    if (gateStr == "buf")  return GateType::BUF;
    if (gateStr == "not")  return GateType::NOT;
    throw SyntaxException("Unknown gate primitive target: '" + gateStr + "'", line, col);
}

bool Parser::parseNetlist(const std::string& verilogCode) {
    m_inputs.clear(); m_outputs.clear(); m_wires.clear(); m_gates.clear(); m_wireStates.clear();
    std::vector<Token> tokens;
    tokenize(verilogCode, tokens);

    size_t i = 0;
    while (i < tokens.size()) {
        if (tokens[i].value == "module") {
            i++; // Advance past 'module'
            if (i < tokens.size()) {
                // Skips module name identifier safely
                i++; 
            }
            
            // Parse tokens inside the module header declaration safely before terminal ';'
            while (i < tokens.size() && tokens[i].value != ";") {
                if (tokens[i].value == "input" || tokens[i].value == "output") {
                    std::string type = tokens[i].value;
                    i++; // Move to the port identifier name
                    
                    if (i < tokens.size() && tokens[i].value != "," && tokens[i].value != ")") {
                        if (type == "input") m_inputs.push_back(tokens[i].value);
                        else if (type == "output") m_outputs.push_back(tokens[i].value);
                        
                        m_wireStates[tokens[i].value] = LogicState::HighImpedance;
                    }
                }
                i++;
            }
        } else if (tokens[i].value == "input" || tokens[i].value == "output" || tokens[i].value == "wire") {
            std::string type = tokens[i].value;
            size_t startLine = tokens[i].line;
            size_t startCol = tokens[i].column;
            i++; // Advance past 'input'/'output'/'wire'
            
            bool closedCleanly = false;
            while (i < tokens.size()) {
                if (tokens[i].value == ";") {
                    closedCleanly = true;
                    break;
                }
                
                // REPAIR: If we hit a gate primitive or another keyword inside the wire block,
                // it implies the previous statement wasn't closed with a semicolon.
                std::string val = tokens[i].value;
                if (val == "and"  || val == "or"  || val == "xor" || val == "nand" ||
                    val == "nor"  || val == "xnor" || val == "buf" || val == "not"  ||
                    val == "wire" || val == "input"|| val == "output"|| val == "endmodule") {
                    break; // Force-break to trigger the Unterminated SyntaxException below
                }
                
                if (tokens[i].value != ",") {
                    if (type == "input") m_inputs.push_back(tokens[i].value);
                    else if (type == "output") m_outputs.push_back(tokens[i].value);
                    else if (type == "wire") m_wires.push_back(tokens[i].value);
                    
                    m_wireStates[tokens[i].value] = LogicState::HighImpedance;
                }
                i++;
            }
            if (!closedCleanly) {
                throw SyntaxException("Unterminated keyword block sequence, expected terminal ';'", startLine, startCol);
            }
        } else if (tokens[i].value == "and" || tokens[i].value == "or" || tokens[i].value == "xor" ||
                   tokens[i].value == "nand" || tokens[i].value == "nor" || tokens[i].value == "xnor" ||
                   tokens[i].value == "buf" || tokens[i].value == "not") {
            
            Gate gate;
            gate.type = stringToGateType(tokens[i].value, tokens[i].line, tokens[i].column);
            i++; 

            if (i >= tokens.size() || tokens[i].value == "(") {
                throw SyntaxException("Anonymous gate instantiation block discovered. Gate name required", tokens[i-1].line, tokens[i-1].column);
            }
            
            gate.name = tokens[i].value;
            i++; 
            
            if (i >= tokens.size() || tokens[i].value != "(") {
                throw SyntaxException("Expected pin list opening boundary wrapper '('", tokens[i].line, tokens[i].column);
            }
            i++;

            std::vector<std::string> gateWires;
            while (i < tokens.size() && tokens[i].value != ")") {
                if (tokens[i].value != ",") {
                    gateWires.push_back(tokens[i].value);
                }
                i++;
            }
            
            if (i >= tokens.size() || tokens[i].value != ")") {
                // REPAIR: Use the active token tracking metrics instead of the gate structure fields
                throw SyntaxException("Unbalanced syntax encapsulation, missing wire closing boundary wrapper ')'", tokens[i-1].line, tokens[i-1].column); 
            }
            i++; // Advance beyond ')'

            if (i >= tokens.size() || tokens[i].value != ";") {
                throw SyntaxException("Missing required instruction termination sequence operator ';'", tokens[i-1].line, tokens[i-1].column);
            }

            if (gateWires.empty()) {
                throw SyntaxException("Empty component structural net connections mapping inside pin list instantiation", tokens[i].line, tokens[i].column);
            }
            
            // Safe multi-port checking using robust vector boundary assertions
            gate.outputWire = gateWires.at(0);
            for (size_t k = 1; k < gateWires.size(); ++k) {
                gate.inputWires.push_back(gateWires.at(k));
            }
            m_gates.push_back(gate);
        }
        i++;
    }
    return true;
}

// Logic evaluation and accessors remain identical, guarding runtime execution flows
LogicState Parser::evaluateGate(GateType type, const std::vector<LogicState>& inputs) const {
    if (inputs.empty()) return LogicState::HighImpedance;
    for (auto state : inputs) {
        if (state == LogicState::HighImpedance) return LogicState::HighImpedance;
    }
    auto to_bool = [](LogicState s) { return s == LogicState::LogicHigh; };
    auto to_state = [](bool b) { return b ? LogicState::LogicHigh : LogicState::LogicLow; };

    switch (type) {
        case GateType::AND:  { bool res = true;  for (auto in : inputs) res &= to_bool(in); return to_state(res); }
        case GateType::OR:   { bool res = false; for (auto in : inputs) res |= to_bool(in); return to_state(res); }
        case GateType::XOR:  { bool res = false; for (auto in : inputs) res ^= to_bool(in); return to_state(res); }
        case GateType::NAND: { bool res = true;  for (auto in : inputs) res &= to_bool(in); return to_state(!res); }
        case GateType::NOR:  { bool res = false; for (auto in : inputs) res |= to_bool(in); return to_state(!res); }
        case GateType::XNOR: { bool res = false; for (auto in : inputs) res ^= to_bool(in); return to_state(!res); }
        case GateType::BUF:  return inputs.at(0);
        case GateType::NOT:  return to_state(!to_bool(inputs.at(0)));
    }
    return LogicState::HighImpedance;
}

void Parser::evaluateCircuit() {
    bool changed = true;
    int maxIterations = 100; // Prevent infinite loops in cyclic combinational paths

    while (changed && maxIterations-- > 0) {
        changed = false;
        
        for (const auto& gate : m_gates) {
            std::vector<LogicState> inputStates;
            
            // Extract and verify input wire states defensively
            for (const auto& inWire : gate.inputWires) {
                auto it = m_wireStates.find(inWire);
                if (it == m_wireStates.end()) {
                    // Fallback registration if the wire declaration was skipped or missed
                    m_wireStates[inWire] = LogicState::HighImpedance;
                    inputStates.push_back(LogicState::HighImpedance);
                } else {
                    inputStates.push_back(it->second);
                }
            }
            
            // Compute the output tri-state value based on the gate logic table
            LogicState newOutput = evaluateGate(gate.type, inputStates);
            
            // Read current output state safely using the map iterator
            auto outIt = m_wireStates.find(gate.outputWire);
            if (outIt == m_wireStates.end()) {
                // Wire didn't exist in mapping; initialize and trigger propagation change
                m_wireStates[gate.outputWire] = newOutput;
                changed = true;
            } else if (outIt->second != newOutput) {
                // Wire state flipped; update value and reinforce simulation loop
                outIt->second = newOutput;
                changed = true;
            }
        }
    }
}

void Parser::setInputState(const std::string& inputName, LogicState state) { m_wireStates.at(inputName) = state; }
LogicState Parser::getWireState(const std::string& wireName) const { return m_wireStates.at(wireName); }

} // namespace MiniEDA