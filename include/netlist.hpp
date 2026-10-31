#pragma once
#include <optional>
#include <string>
#include <vector>

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

// One parsed Verilog module: plain data shared by the simulator and the schematic view.
// Vector nets are already expanded into scalar bits ("a[1]", "a[0]").
struct Netlist {
    std::string moduleName;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    std::vector<std::string> wires;
    std::vector<Gate> gates;
};

// Lower-case Verilog primitive name ("and", "nor", ...)
inline const char* gateTypeName(GateType type) {
    switch (type) {
        case GateType::AND:  return "and";
        case GateType::OR:   return "or";
        case GateType::XOR:  return "xor";
        case GateType::NAND: return "nand";
        case GateType::NOR:  return "nor";
        case GateType::XNOR: return "xnor";
        case GateType::BUF:  return "buf";
        case GateType::NOT:  return "not";
    }
    return "unknown";
}

// Inverse of gateTypeName; empty when the word is not a gate primitive
inline std::optional<GateType> gateTypeFromName(const std::string& name) {
    for (GateType type : {GateType::AND, GateType::OR, GateType::XOR, GateType::NAND,
                          GateType::NOR, GateType::XNOR, GateType::BUF, GateType::NOT}) {
        if (name == gateTypeName(type)) return type;
    }
    return std::nullopt;
}

} // namespace MiniEDA
