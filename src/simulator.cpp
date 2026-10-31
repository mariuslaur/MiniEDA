#include "simulator.hpp"
#include <algorithm>

namespace MiniEDA {

Simulator::Simulator(Netlist netlist) : m_netlist(std::move(netlist)) {
    // Register every net the design mentions, including implicitly declared ones
    auto registerNet = [this](const std::string& net) { m_wireStates.emplace(net, LogicState::HighImpedance); };
    for (const auto& net : m_netlist.inputs) registerNet(net);
    for (const auto& net : m_netlist.outputs) registerNet(net);
    for (const auto& net : m_netlist.wires) registerNet(net);
    for (const auto& gate : m_netlist.gates) {
        registerNet(gate.outputWire);
        for (const auto& net : gate.inputWires) registerNet(net);
    }
}

void Simulator::setInput(const std::string& net, LogicState state) {
    m_wireStates.at(net) = state;
}

LogicState Simulator::wireState(const std::string& net) const {
    return m_wireStates.at(net);
}

LogicState Simulator::evaluateGate(GateType type, const std::vector<LogicState>& inputs) {
    if (inputs.empty()) return LogicState::HighImpedance;

    // A controlling value decides the output even when other inputs are still Z
    // (0 for AND/NAND, 1 for OR/NOR). Without this, cross-coupled latches never leave Z.
    auto hasInput = [&](LogicState s) { return std::find(inputs.begin(), inputs.end(), s) != inputs.end(); };
    switch (type) {
        case GateType::AND:  if (hasInput(LogicState::LogicLow))  return LogicState::LogicLow;  break;
        case GateType::NAND: if (hasInput(LogicState::LogicLow))  return LogicState::LogicHigh; break;
        case GateType::OR:   if (hasInput(LogicState::LogicHigh)) return LogicState::LogicHigh; break;
        case GateType::NOR:  if (hasInput(LogicState::LogicHigh)) return LogicState::LogicLow;  break;
        default: break;
    }
    if (hasInput(LogicState::HighImpedance)) return LogicState::HighImpedance;

    auto toBool = [](LogicState s) { return s == LogicState::LogicHigh; };
    auto toState = [](bool b) { return b ? LogicState::LogicHigh : LogicState::LogicLow; };

    switch (type) {
        case GateType::AND:  { bool res = true;  for (auto in : inputs) res &= toBool(in); return toState(res); }
        case GateType::OR:   { bool res = false; for (auto in : inputs) res |= toBool(in); return toState(res); }
        case GateType::XOR:  { bool res = false; for (auto in : inputs) res ^= toBool(in); return toState(res); }
        case GateType::NAND: { bool res = true;  for (auto in : inputs) res &= toBool(in); return toState(!res); }
        case GateType::NOR:  { bool res = false; for (auto in : inputs) res |= toBool(in); return toState(!res); }
        case GateType::XNOR: { bool res = false; for (auto in : inputs) res ^= toBool(in); return toState(!res); }
        case GateType::BUF:  return inputs.at(0);
        case GateType::NOT:  return toState(!toBool(inputs.at(0)));
    }
    return LogicState::HighImpedance;
}

void Simulator::evaluate() {
    bool changed = true;
    int maxIterations = 100; // Prevent infinite loops in oscillating feedback paths

    std::vector<LogicState> inputStates;
    while (changed && maxIterations-- > 0) {
        changed = false;
        for (const auto& gate : m_netlist.gates) {
            inputStates.clear();
            for (const auto& net : gate.inputWires) inputStates.push_back(m_wireStates.at(net));

            LogicState newOutput = evaluateGate(gate.type, inputStates);
            LogicState& output = m_wireStates.at(gate.outputWire);
            if (output != newOutput) {
                output = newOutput;
                changed = true;
            }
        }
    }
}

} // namespace MiniEDA
