#pragma once
#include "circuitItems.hpp"
#include "simulator.hpp"
#include <QGraphicsPathItem>
#include <QGraphicsScene>

namespace MiniEDA {

// Draws a simulated netlist (module frame, ports, gates, wires) and keeps direct pointers to every
// item, so a refresh after each simulation step re-colours them without searching the scene.
class SchematicScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit SchematicScene(QObject* parent = nullptr);

    // Rebuilds all items for the simulator's netlist; the module is centered in viewSize when it fits
    void build(const Simulator& simulator, const QSizeF& viewSize);

    // Re-colours wires, gates and ports from the current simulation state
    void refresh(const Simulator& simulator);

signals:
    // The user clicked an input port; `state` is its new value
    void inputToggled(const std::string& net, MiniEDA::LogicState state);

private:
    struct WireItem {
        QGraphicsPathItem* item;
        std::string net;
    };
    std::vector<WireItem> m_wires;
    std::vector<GateItem*> m_gates; // Same order as Netlist::gates
    std::vector<InputPortItem*> m_inputs;
    std::vector<OutputPortItem*> m_outputs;
};

} // namespace MiniEDA
