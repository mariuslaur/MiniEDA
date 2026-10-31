#include "schematicScene.hpp"
#include "schematicLayout.hpp"
#include <QPainterPath>
#include <algorithm>

namespace MiniEDA {

namespace {
const qreal kSceneMargin = 20.0; // Space kept around the circuit in the scrollable area
}

SchematicScene::SchematicScene(QObject* parent) : QGraphicsScene(parent) {}

void SchematicScene::build(const Simulator& simulator, const QSizeF& viewSize) {
    clear();
    m_wires.clear();
    m_gates.clear();
    m_inputs.clear();
    m_outputs.clear();

    const Netlist& netlist = simulator.netlist();

    // Every box in a category fits the largest label of that category
    LayoutMetrics metrics;
    metrics.viewSize = viewSize;
    metrics.inputBoxWidth = 0;
    metrics.outputBoxWidth = 0;
    for (const auto& net : netlist.inputs) metrics.inputBoxWidth = std::max(metrics.inputBoxWidth, InputPortItem::labelWidth(net));
    for (const auto& net : netlist.outputs) metrics.outputBoxWidth = std::max(metrics.outputBoxWidth, OutputPortItem::labelWidth(net));

    QSizeF gateLabel(0, 0);
    size_t maxGatePins = 1;
    for (const auto& gate : netlist.gates) {
        gateLabel = gateLabel.expandedTo(GateItem::labelSize(gate));
        maxGatePins = std::max(maxGatePins, gate.inputWires.size());
    }
    metrics.gateWidth = gateLabel.width();
    // Tall enough for the label and for at least 10px between neighbouring input pins
    metrics.gateHeight = std::max(gateLabel.height(), (maxGatePins + 1) * 10.0);

    const SchematicLayout layout = computeSchematicLayout(netlist, metrics);

    // Module frame and title
    addRect(layout.moduleFrame, QPen(QColor(80, 80, 80), 2, Qt::DashLine), QBrush(QColor(253, 253, 253)));
    const std::string title = netlist.moduleName.empty() ? "(unnamed)" : netlist.moduleName;
    auto* titleItem = addText(QString::fromStdString("Module: " + title), QFont("Arial", 11, QFont::Bold));
    titleItem->setPos(layout.titlePos);

    // Wires first, so ports and gates are drawn on top of their pin ends
    for (const auto& wire : layout.wires) {
        QPainterPath path;
        for (const auto& branch : wire.branches) {
            path.moveTo(branch.front());
            for (size_t i = 1; i < branch.size(); ++i) path.lineTo(branch[i]);
        }
        m_wires.push_back({addPath(path, Palette::wirePen(LogicState::HighImpedance)), wire.net});
    }

    for (size_t g = 0; g < netlist.gates.size(); ++g) {
        auto* item = new GateItem(netlist.gates[g], layout.gates[g]);
        addItem(item);
        m_gates.push_back(item);
    }

    for (const auto& port : layout.inputs) {
        auto* item = new InputPortItem(port.net, port.box);
        item->onToggled = [this](const std::string& net, LogicState state) { emit inputToggled(net, state); };
        addItem(item);
        m_inputs.push_back(item);
    }

    for (const auto& port : layout.outputs) {
        auto* item = new OutputPortItem(port.net, port.box);
        addItem(item);
        m_outputs.push_back(item);
    }

    refresh(simulator);

    // Scrollable area = circuit bounds, so "Fit" leaves no stray scrollbars
    setSceneRect(itemsBoundingRect().adjusted(-kSceneMargin, -kSceneMargin, kSceneMargin, kSceneMargin));
}

void SchematicScene::refresh(const Simulator& simulator) {
    for (const auto& wire : m_wires) {
        wire.item->setPen(Palette::wirePen(simulator.wireState(wire.net)));
    }

    const auto& gates = simulator.netlist().gates;
    std::vector<LogicState> pinStates;
    for (size_t g = 0; g < m_gates.size(); ++g) {
        pinStates.clear();
        for (const auto& net : gates[g].inputWires) pinStates.push_back(simulator.wireState(net));
        m_gates[g]->setStates(pinStates, simulator.wireState(gates[g].outputWire));
    }

    for (auto* input : m_inputs) input->setState(simulator.wireState(input->net()));
    for (auto* output : m_outputs) output->setState(simulator.wireState(output->net()));
}

} // namespace MiniEDA
