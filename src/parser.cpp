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

        // Skip '//' line comments and '/* */' block comments, keeping line/column tracking intact
        if (ch == '/' && i + 1 < code.length() && (code[i + 1] == '/' || code[i + 1] == '*')) {
            if (!current.empty()) {
                tokens.push_back({current, currentLine, tokenStartColumn});
                current.clear();
            }
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
        } else if (ch == ';' || ch == ',' || ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == ':' ||
                   ch == '=' || ch == '&' || ch == '|' || ch == '^' || ch == '~' || ch == '!') {
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

void Parser::addPort(std::vector<std::string>& list, const std::string& name) {
    if (std::find(list.begin(), list.end(), name) == list.end()) {
        list.push_back(name);
    }
    m_wireStates[name] = LogicState::HighImpedance;
}

namespace {

// Token at position i, or the last token when i ran past the end (for error coordinates)
const Token& tokenAt(const std::vector<Token>& tokens, size_t i) {
    return tokens[std::min(i, tokens.size() - 1)];
}

bool isDelimiter(const std::string& val) {
    return val.size() == 1 && std::string(";,()[]:=&|^~!").find(val[0]) != std::string::npos;
}

bool isReserved(const std::string& val) {
    return val == "module" || val == "endmodule" || val == "input" || val == "output" || val == "wire" ||
           val == "reg" || val == "assign" || val == "and" || val == "or" || val == "xor" || val == "nand" ||
           val == "nor" || val == "xnor" || val == "buf" || val == "not";
}

bool isNumber(const std::string& val) {
    return !val.empty() && std::all_of(val.begin(), val.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
}

// True when tokens i and i+1 are `first` and `second` written without a gap (e.g. "~^", "&&")
bool isAdjacentPair(const std::vector<Token>& tokens, size_t i, const char* first, const char* second) {
    return i + 1 < tokens.size() && tokens[i].value == first && tokens[i + 1].value == second &&
           tokens[i].line == tokens[i + 1].line && tokens[i + 1].column == tokens[i].column + 1;
}

} // namespace

void Parser::declareNets(std::vector<std::string>& list, const std::string& name, bool isVector, int msb, int lsb) {
    if (!isVector) {
        addPort(list, name);
        return;
    }
    m_vectorRanges[name] = {msb, lsb};
    int step = (msb >= lsb) ? -1 : 1;
    for (int bit = msb; ; bit += step) {
        addPort(list, name + "[" + std::to_string(bit) + "]");
        if (bit == lsb) break;
    }
}

bool Parser::parseRange(const std::vector<Token>& tokens, size_t& i, int& msb, int& lsb) const {
    if (i >= tokens.size() || tokens[i].value != "[") return false;
    const Token& open = tokens[i];
    if (i + 4 >= tokens.size() || !isNumber(tokens[i + 1].value) || tokens[i + 2].value != ":" ||
        !isNumber(tokens[i + 3].value) || tokens[i + 4].value != "]") {
        throw SyntaxException("Malformed vector range, expected '[msb:lsb]'", open.line, open.column);
    }
    msb = std::stoi(tokens[i + 1].value);
    lsb = std::stoi(tokens[i + 3].value);
    i += 5;
    return true;
}

std::string Parser::parseNetRef(const std::vector<Token>& tokens, size_t& i) const {
    if (i >= tokens.size() || isDelimiter(tokens[i].value) || isReserved(tokens[i].value)) {
        const Token& errTok = tokenAt(tokens, i);
        throw SyntaxException("Expected net identifier", errTok.line, errTok.column);
    }
    const Token& nameTok = tokens[i];
    std::string name = nameTok.value;
    i++;

    auto range = m_vectorRanges.find(name);
    if (i < tokens.size() && tokens[i].value == "[") {
        if (i + 2 >= tokens.size() || !isNumber(tokens[i + 1].value) || tokens[i + 2].value != "]") {
            throw SyntaxException("Malformed bit select, expected '[index]'", tokens[i].line, tokens[i].column);
        }
        int bit = std::stoi(tokens[i + 1].value);
        if (range != m_vectorRanges.end()) {
            int lo = std::min(range->second.first, range->second.second);
            int hi = std::max(range->second.first, range->second.second);
            if (bit < lo || bit > hi) {
                throw SyntaxException("Bit select '" + name + "[" + std::to_string(bit) + "]' is outside the declared range",
                                      tokens[i + 1].line, tokens[i + 1].column);
            }
        }
        i += 3;
        return name + "[" + std::to_string(bit) + "]";
    }
    if (range != m_vectorRanges.end()) {
        throw SyntaxException("Vector net '" + name + "' requires a bit select", nameTok.line, nameTok.column);
    }
    return name;
}

std::string Parser::emitAssignGate(GateType type, const std::vector<std::string>& inputs, size_t line) {
    ++m_assignCounter;
    Gate gate;
    gate.name = "assign_L" + std::to_string(line) + "_" + std::to_string(m_assignCounter);
    gate.type = type;
    gate.outputWire = "$n" + std::to_string(m_assignCounter);
    gate.inputWires = inputs;
    m_wireStates[gate.outputWire] = LogicState::HighImpedance;
    m_gates.push_back(gate);
    return gate.outputWire;
}

// Verilog precedence (1-bit): unary ~ ! > & && > ^ ~^ > | ||
std::string Parser::parseOrExpr(const std::vector<Token>& tokens, size_t& i, size_t line) {
    std::vector<std::string> operands{parseXorExpr(tokens, i, line)};
    while (i < tokens.size() && tokens[i].value == "|") {
        i += isAdjacentPair(tokens, i, "|", "|") ? 2 : 1; // '||' behaves like '|' on 1-bit nets
        operands.push_back(parseXorExpr(tokens, i, line));
    }
    return operands.size() == 1 ? operands[0] : emitAssignGate(GateType::OR, operands, line);
}

std::string Parser::parseXorExpr(const std::vector<Token>& tokens, size_t& i, size_t line) {
    std::vector<std::string> operands{parseAndExpr(tokens, i, line)};
    while (i < tokens.size()) {
        bool xnorOp = false;
        if (isAdjacentPair(tokens, i, "~", "^") || isAdjacentPair(tokens, i, "^", "~")) {
            xnorOp = true;
            i += 2;
        } else if (tokens[i].value == "^") {
            i++;
        } else {
            break;
        }
        std::string rhs = parseAndExpr(tokens, i, line);
        if (xnorOp) {
            // An n-input XNOR is not a chain of 2-input XNORs, so each '~^' gets its own gate
            std::string lhs = operands.size() == 1 ? operands[0] : emitAssignGate(GateType::XOR, operands, line);
            operands = {emitAssignGate(GateType::XNOR, {lhs, rhs}, line)};
        } else {
            operands.push_back(rhs);
        }
    }
    return operands.size() == 1 ? operands[0] : emitAssignGate(GateType::XOR, operands, line);
}

std::string Parser::parseAndExpr(const std::vector<Token>& tokens, size_t& i, size_t line) {
    std::vector<std::string> operands{parseUnaryExpr(tokens, i, line)};
    while (i < tokens.size() && tokens[i].value == "&") {
        i += isAdjacentPair(tokens, i, "&", "&") ? 2 : 1; // '&&' behaves like '&' on 1-bit nets
        operands.push_back(parseUnaryExpr(tokens, i, line));
    }
    return operands.size() == 1 ? operands[0] : emitAssignGate(GateType::AND, operands, line);
}

std::string Parser::parseUnaryExpr(const std::vector<Token>& tokens, size_t& i, size_t line) {
    if (i < tokens.size() && (tokens[i].value == "~" || tokens[i].value == "!")) {
        i++;
        return emitAssignGate(GateType::NOT, {parseUnaryExpr(tokens, i, line)}, line);
    }
    if (i < tokens.size() && tokens[i].value == "(") {
        const Token& open = tokens[i];
        i++;
        std::string inner = parseOrExpr(tokens, i, line);
        if (i >= tokens.size() || tokens[i].value != ")") {
            throw SyntaxException("Unbalanced parenthesis in expression, missing ')'", open.line, open.column);
        }
        i++;
        return inner;
    }
    return parseNetRef(tokens, i);
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
    m_vectorRanges.clear(); m_assignCounter = 0;
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
            // ANSI-style headers: the direction keyword applies to every following identifier
            // until the next direction keyword (e.g. "input a, b, output y, z")
            std::string direction;
            bool isVector = false;
            int msb = 0, lsb = 0;
            while (i < tokens.size() && tokens[i].value != ";") {
                const std::string& val = tokens[i].value;
                if (val == "input" || val == "output") {
                    direction = val;
                    isVector = false;
                } else if (val == "[") {
                    // Range applies to all following names of the current direction
                    isVector = parseRange(tokens, i, msb, lsb);
                    continue;
                } else if (val != "," && val != "(" && val != ")" && val != "wire" && val != "reg") {
                    if (direction == "input") declareNets(m_inputs, val, isVector, msb, lsb);
                    else if (direction == "output") declareNets(m_outputs, val, isVector, msb, lsb);
                }
                i++;
            }
        } else if (tokens[i].value == "input" || tokens[i].value == "output" || tokens[i].value == "wire") {
            std::string type = tokens[i].value;
            size_t startLine = tokens[i].line;
            size_t startCol = tokens[i].column;
            i++; // Advance past 'input'/'output'/'wire'
            
            bool closedCleanly = false;
            bool isVector = false;
            int msb = 0, lsb = 0;
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
                    val == "wire" || val == "input"|| val == "output"|| val == "endmodule" || val == "assign") {
                    break; // Force-break to trigger the Unterminated SyntaxException below
                }

                if (val == "[") {
                    isVector = parseRange(tokens, i, msb, lsb);
                    continue;
                }

                if (tokens[i].value != ",") {
                    if (type == "input") declareNets(m_inputs, val, isVector, msb, lsb);
                    else if (type == "output") declareNets(m_outputs, val, isVector, msb, lsb);
                    else if (type == "wire") declareNets(m_wires, val, isVector, msb, lsb);
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
                // Fall back to the last valid token when the input ends right after the gate name
                const Token& errTok = (i < tokens.size()) ? tokens[i] : tokens[i-1];
                throw SyntaxException("Expected pin list opening boundary wrapper '('", errTok.line, errTok.column);
            }
            i++;

            std::vector<std::string> gateWires;
            while (i < tokens.size() && tokens[i].value != ")") {
                if (tokens[i].value == ",") {
                    i++;
                    continue;
                }
                gateWires.push_back(parseNetRef(tokens, i)); // Accepts scalar nets and bit selects "a[0]"
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
        } else if (tokens[i].value == "assign") {
            // Continuous assignment: "assign lhs = <boolean expression>;" is lowered into gates
            size_t line = tokens[i].line;
            i++;
            std::string lhs = parseNetRef(tokens, i);

            if (i >= tokens.size() || tokens[i].value != "=") {
                const Token& errTok = tokenAt(tokens, i);
                throw SyntaxException("Expected '=' in continuous assignment", errTok.line, errTok.column);
            }
            i++;

            size_t gatesBefore = m_gates.size();
            std::string rhs = parseOrExpr(tokens, i, line);

            if (i >= tokens.size() || tokens[i].value != ";") {
                const Token& errTok = tokenAt(tokens, i);
                throw SyntaxException("Missing required instruction termination sequence operator ';'", errTok.line, errTok.column);
            }

            if (m_gates.size() > gatesBefore && m_gates.back().outputWire == rhs) {
                // The expression's root gate drives the target directly, no extra buffer needed
                m_wireStates.erase(rhs);
                m_gates.back().outputWire = lhs;
            } else {
                // Plain net-to-net assignment ("assign y = w;") becomes a buffer
                ++m_assignCounter;
                m_gates.push_back({"assign_L" + std::to_string(line) + "_" + std::to_string(m_assignCounter),
                                   GateType::BUF, lhs, {rhs}});
            }
            m_wireStates.try_emplace(lhs, LogicState::HighImpedance);
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