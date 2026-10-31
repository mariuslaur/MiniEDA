#pragma once
#include "netlist.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace MiniEDA {

// Tri-state (0 / 1 / Z) simulation of a Netlist. Every net starts at Z.
// Controlling values (0 on AND/NAND, 1 on OR/NOR) resolve outputs even with Z inputs,
// so cross-coupled latches and flip-flops settle into defined states.
class Simulator {
public:
    explicit Simulator(Netlist netlist);

    const Netlist& netlist() const { return m_netlist; }

    // Drives a net, normally a primary input. Throws std::out_of_range for unknown nets.
    void setInput(const std::string& net, LogicState state);

    // Propagates signals until the circuit settles (bounded, so oscillating loops terminate)
    void evaluate();

    // Throws std::out_of_range for unknown nets
    LogicState wireState(const std::string& net) const;

    static LogicState evaluateGate(GateType type, const std::vector<LogicState>& inputs);

private:
    Netlist m_netlist;
    std::unordered_map<std::string, LogicState> m_wireStates;
};

} // namespace MiniEDA
