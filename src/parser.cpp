#include "parser.hpp"
#include <cctype>

namespace MiniEDA {

namespace {

// Words that can never be net names; one of them inside a declaration means a missing ';'
bool isKeyword(const std::string& value) {
    return value == "module" || value == "endmodule" || value == "input" || value == "output" ||
           value == "wire" || value == "reg" || value == "assign" || gateTypeFromName(value).has_value();
}

bool isNumber(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(),
                                         [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
}

} // namespace

Netlist Parser::parse(const std::string& verilogCode) {
    m_tokens = tokenize(verilogCode);
    m_pos = 0;
    m_netlist = Netlist{};
    m_vectorRanges.clear();
    m_assignCounter = 0;

    while (!atEnd()) {
        const std::string& word = m_tokens[m_pos].value;
        if (word == "module") {
            parseModuleHeader();
        } else if (word == "input" || word == "output" || word == "wire") {
            parseDeclaration();
        } else if (gateTypeFromName(word)) {
            parseGateInstance();
        } else if (word == "assign") {
            parseAssign();
        }
        m_pos++; // Past the terminating ';' (or an ignored token such as "endmodule")
    }
    return std::move(m_netlist);
}

// ---- Statements ----

void Parser::parseModuleHeader() {
    const Token& moduleTok = m_tokens[m_pos];
    m_pos++; // Advance past 'module'
    if (atEnd() || isDelimiter(m_tokens[m_pos].value) || isKeyword(m_tokens[m_pos].value)) {
        fail("Module name required after 'module'", moduleTok);
    }
    m_netlist.moduleName = m_tokens[m_pos].value;
    m_pos++;

    // ANSI-style headers: the direction keyword (and optional range) applies to every following
    // identifier until the next direction keyword (e.g. "input a, b, output [1:0] y")
    std::string direction;
    bool isVector = false;
    int msb = 0, lsb = 0;
    while (!atEnd() && m_tokens[m_pos].value != ";") {
        const std::string& val = m_tokens[m_pos].value;
        if (val == "input" || val == "output") {
            direction = val;
            isVector = false;
        } else if (val == "[") {
            isVector = parseRange(msb, lsb);
            continue;
        } else if (val != "," && val != "(" && val != ")" && val != "wire" && val != "reg") {
            if (direction == "input") declareNets(m_netlist.inputs, val, isVector, msb, lsb);
            else if (direction == "output") declareNets(m_netlist.outputs, val, isVector, msb, lsb);
        }
        m_pos++;
    }
}

void Parser::parseDeclaration() {
    const Token& keyword = m_tokens[m_pos];
    std::vector<std::string>& target = keyword.value == "input"  ? m_netlist.inputs
                                     : keyword.value == "output" ? m_netlist.outputs
                                                                 : m_netlist.wires;
    m_pos++; // Advance past 'input'/'output'/'wire'

    bool isVector = false;
    int msb = 0, lsb = 0;
    while (!atEnd()) {
        const std::string& val = m_tokens[m_pos].value;
        if (val == ";") return;
        // A keyword here means the declaration was not closed with a semicolon
        if (isKeyword(val)) break;
        if (val == "[") {
            isVector = parseRange(msb, lsb);
            continue;
        }
        if (val != ",") declareNets(target, val, isVector, msb, lsb);
        m_pos++;
    }
    fail("Unterminated keyword block sequence, expected terminal ';'", keyword);
}

void Parser::parseGateInstance() {
    const Token& keyword = m_tokens[m_pos];
    Gate gate;
    gate.type = *gateTypeFromName(keyword.value);
    m_pos++;

    if (atEnd() || m_tokens[m_pos].value == "(") {
        fail("Anonymous gate instantiation block discovered. Gate name required", keyword);
    }
    gate.name = m_tokens[m_pos].value;
    m_pos++;

    if (!peekIs("(")) {
        fail("Expected pin list opening boundary wrapper '('", current());
    }
    m_pos++;

    std::vector<std::string> pins;
    while (!atEnd() && m_tokens[m_pos].value != ")") {
        if (m_tokens[m_pos].value == ",") {
            m_pos++;
            continue;
        }
        pins.push_back(parseNetRef()); // Accepts scalar nets and bit selects "a[0]"
    }
    if (atEnd()) {
        fail("Unbalanced syntax encapsulation, missing wire closing boundary wrapper ')'", current());
    }
    m_pos++; // Advance beyond ')'

    if (!peekIs(";")) {
        fail("Missing required instruction termination sequence operator ';'", m_tokens[m_pos - 1]);
    }
    if (pins.empty()) {
        fail("Empty component structural net connections mapping inside pin list instantiation", m_tokens[m_pos]);
    }

    // First pin is the output, the rest are inputs
    gate.outputWire = pins.front();
    gate.inputWires.assign(pins.begin() + 1, pins.end());
    m_netlist.gates.push_back(std::move(gate));
}

void Parser::parseAssign() {
    // Continuous assignment: "assign lhs = <boolean expression>;" is lowered into gates
    size_t line = m_tokens[m_pos].line;
    m_pos++;
    std::string lhs = parseNetRef();

    if (!peekIs("=")) fail("Expected '=' in continuous assignment", current());
    m_pos++;

    auto& gates = m_netlist.gates;
    size_t gatesBefore = gates.size();
    std::string rhs = parseOrExpr(line);

    if (!peekIs(";")) fail("Missing required instruction termination sequence operator ';'", current());

    if (gates.size() > gatesBefore && gates.back().outputWire == rhs) {
        // The expression's root gate drives the target directly, no extra buffer needed
        gates.back().outputWire = lhs;
    } else {
        // Plain net-to-net assignment ("assign y = w;") becomes a buffer
        ++m_assignCounter;
        gates.push_back({"assign_L" + std::to_string(line) + "_" + std::to_string(m_assignCounter),
                         GateType::BUF, lhs, {rhs}});
    }
}

// ---- Nets and vectors ----

void Parser::declareNets(std::vector<std::string>& list, const std::string& name, bool isVector, int msb, int lsb) {
    auto addUnique = [&list](const std::string& net) {
        if (std::find(list.begin(), list.end(), net) == list.end()) list.push_back(net);
    };
    if (!isVector) {
        addUnique(name);
        return;
    }
    m_vectorRanges[name] = {msb, lsb};
    int step = (msb >= lsb) ? -1 : 1;
    for (int bit = msb; ; bit += step) {
        addUnique(name + "[" + std::to_string(bit) + "]");
        if (bit == lsb) break;
    }
}

bool Parser::parseRange(int& msb, int& lsb) {
    if (!peekIs("[")) return false;
    const Token& open = m_tokens[m_pos];
    size_t i = m_pos;
    if (i + 4 >= m_tokens.size() || !isNumber(m_tokens[i + 1].value) || m_tokens[i + 2].value != ":" ||
        !isNumber(m_tokens[i + 3].value) || m_tokens[i + 4].value != "]") {
        fail("Malformed vector range, expected '[msb:lsb]'", open);
    }
    msb = std::stoi(m_tokens[i + 1].value);
    lsb = std::stoi(m_tokens[i + 3].value);
    m_pos += 5;
    return true;
}

std::string Parser::parseNetRef() {
    if (atEnd() || isDelimiter(m_tokens[m_pos].value) || isKeyword(m_tokens[m_pos].value)) {
        fail("Expected net identifier", current());
    }
    const Token& nameTok = m_tokens[m_pos];
    const std::string& name = nameTok.value;
    m_pos++;

    auto range = m_vectorRanges.find(name);
    if (peekIs("[")) {
        size_t i = m_pos;
        if (i + 2 >= m_tokens.size() || !isNumber(m_tokens[i + 1].value) || m_tokens[i + 2].value != "]") {
            fail("Malformed bit select, expected '[index]'", m_tokens[i]);
        }
        int bit = std::stoi(m_tokens[i + 1].value);
        if (range != m_vectorRanges.end()) {
            int lo = std::min(range->second.first, range->second.second);
            int hi = std::max(range->second.first, range->second.second);
            if (bit < lo || bit > hi) {
                fail("Bit select '" + name + "[" + std::to_string(bit) + "]' is outside the declared range",
                     m_tokens[i + 1]);
            }
        }
        m_pos += 3;
        return name + "[" + std::to_string(bit) + "]";
    }
    if (range != m_vectorRanges.end()) {
        fail("Vector net '" + name + "' requires a bit select", nameTok);
    }
    return name;
}

// ---- assign expressions ----

std::string Parser::emitAssignGate(GateType type, const std::vector<std::string>& inputs, size_t line) {
    ++m_assignCounter;
    Gate gate;
    gate.name = "assign_L" + std::to_string(line) + "_" + std::to_string(m_assignCounter);
    gate.type = type;
    gate.outputWire = "$n" + std::to_string(m_assignCounter);
    gate.inputWires = inputs;
    m_netlist.gates.push_back(gate);
    return gate.outputWire;
}

// Verilog precedence (1-bit): unary ~ ! > & && > ^ ~^ > | ||
std::string Parser::parseOrExpr(size_t line) {
    std::vector<std::string> operands{parseXorExpr(line)};
    while (peekIs("|")) {
        m_pos += peekAdjacentPair("|", "|") ? 2 : 1; // '||' behaves like '|' on 1-bit nets
        operands.push_back(parseXorExpr(line));
    }
    return operands.size() == 1 ? operands[0] : emitAssignGate(GateType::OR, operands, line);
}

std::string Parser::parseXorExpr(size_t line) {
    std::vector<std::string> operands{parseAndExpr(line)};
    while (!atEnd()) {
        bool xnorOp = false;
        if (peekAdjacentPair("~", "^") || peekAdjacentPair("^", "~")) {
            xnorOp = true;
            m_pos += 2;
        } else if (peekIs("^")) {
            m_pos++;
        } else {
            break;
        }
        std::string rhs = parseAndExpr(line);
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

std::string Parser::parseAndExpr(size_t line) {
    std::vector<std::string> operands{parseUnaryExpr(line)};
    while (peekIs("&")) {
        m_pos += peekAdjacentPair("&", "&") ? 2 : 1; // '&&' behaves like '&' on 1-bit nets
        operands.push_back(parseUnaryExpr(line));
    }
    return operands.size() == 1 ? operands[0] : emitAssignGate(GateType::AND, operands, line);
}

std::string Parser::parseUnaryExpr(size_t line) {
    if (peekIs("~") || peekIs("!")) {
        m_pos++;
        return emitAssignGate(GateType::NOT, {parseUnaryExpr(line)}, line);
    }
    if (peekIs("(")) {
        const Token& open = m_tokens[m_pos];
        m_pos++;
        std::string inner = parseOrExpr(line);
        if (!peekIs(")")) {
            fail("Unbalanced parenthesis in expression, missing ')'", open);
        }
        m_pos++;
        return inner;
    }
    return parseNetRef();
}

// ---- Token helpers ----

bool Parser::peekAdjacentPair(const char* first, const char* second) const {
    if (m_pos + 1 >= m_tokens.size()) return false;
    const Token& a = m_tokens[m_pos];
    const Token& b = m_tokens[m_pos + 1];
    return a.value == first && b.value == second && a.line == b.line && b.column == a.column + 1;
}

void Parser::fail(const std::string& message, const Token& at) const {
    throw SyntaxException(message, at.line, at.column);
}

} // namespace MiniEDA
