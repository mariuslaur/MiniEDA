#include "schematicLayout.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace MiniEDA {

namespace {

const qreal kMinItemSpacing = 80.0;   // Vertical distance between items of a column
const qreal kColumnPadding = 60.0;    // Space above the first and below the last item of the tallest column
const qreal kTrackSpacing = 14.0;     // Minimal distance between two parallel wire tracks
const qreal kMinChannelWidth = 150.0; // Routing channel width when only a few tracks are needed
const qreal kBypassClearance = 25.0;  // Distance between the gate column and the first bypass gutter

struct PinRef {
    size_t gate;
    size_t pin;
};

// Smallest shift that keeps a port's horizontal stub off the height of another net's stub
qreal nudgeOffset(qreal pinY, const std::vector<qreal>& foreignYs) {
    for (qreal offset : {0.0, 10.0, -10.0, 20.0, -20.0}) {
        bool clash = std::any_of(foreignYs.begin(), foreignYs.end(),
                                 [&](qreal y) { return std::abs(pinY + offset - y) < 4.0; });
        if (!clash) return offset;
    }
    return 0.0;
}

} // namespace

SchematicLayout computeSchematicLayout(const Netlist& netlist, const LayoutMetrics& m) {
    const auto& inputs = netlist.inputs;
    const auto& gates = netlist.gates;
    const auto& outputs = netlist.outputs;
    const size_t numInputs = inputs.size();
    const size_t numGates = gates.size();
    const size_t numOutputs = outputs.size();
    const qreal itemSpacing = std::max(kMinItemSpacing, m.gateHeight + 15.0);

    // ---- 1. Net topology classification (independent of geometry) ----
    std::map<std::string, size_t> driverGate; // net -> index of the gate driving it
    for (size_t g = 0; g < numGates; ++g) driverGate[gates[g].outputWire] = g;
    std::set<std::string> inputSet(inputs.begin(), inputs.end());

    std::map<std::string, std::vector<PinRef>> inputNetPins; // module input -> gate input pins
    std::map<std::string, std::vector<PinRef>> g2gNetPins;   // gate output  -> gate input pins
    for (size_t g = 0; g < numGates; ++g) {
        for (size_t idx = 0; idx < gates[g].inputWires.size(); ++idx) {
            const auto& net = gates[g].inputWires[idx];
            if (driverGate.count(net)) g2gNetPins[net].push_back({g, idx});
            else if (inputSet.count(net)) inputNetPins[net].push_back({g, idx});
        }
    }

    // Track order: inputs top-to-bottom, gate nets by driver gate, outputs top-to-bottom
    std::vector<std::string> inputNets, g2gNets, outNets;
    for (const auto& in : inputs) if (inputNetPins.count(in)) inputNets.push_back(in);
    for (const auto& gate : gates) if (g2gNetPins.count(gate.outputWire)) g2gNets.push_back(gate.outputWire);
    for (const auto& out : outputs) if (driverGate.count(out)) outNets.push_back(out);

    // Gate-to-gate nets bypass the gate column over the top or under the bottom, each on its own gutter
    const qreal midGate = (static_cast<qreal>(numGates) - 1.0) / 2.0;
    std::map<std::string, bool> g2gViaTop;
    std::map<std::string, size_t> g2gBypassIdx;
    size_t numTop = 0, numBot = 0;
    for (const auto& net : g2gNets) {
        qreal sum = static_cast<qreal>(driverGate[net]);
        for (const auto& pin : g2gNetPins[net]) sum += pin.gate;
        bool viaTop = sum / (g2gNetPins[net].size() + 1.0) <= midGate;
        g2gViaTop[net] = viaTop;
        g2gBypassIdx[net] = viaTop ? numTop++ : numBot++;
    }

    // ---- 2. Module geometry, widened/heightened to fit all tracks and bypass gutters ----
    // Left channel:  [input tracks ... | gate-to-gate left tracks]
    // Right channel: [gate-to-gate right tracks | output tracks ...]
    const size_t leftTracks = inputNets.size() + g2gNets.size();
    const size_t rightTracks = g2gNets.size() + outNets.size();
    const qreal leftChannelWidth = std::max(kMinChannelWidth, (leftTracks + 1) * kTrackSpacing);
    const qreal rightChannelWidth = std::max(kMinChannelWidth, (rightTracks + 1) * kTrackSpacing);

    // The tallest column (with its real item heights) drives the module height
    auto columnHeight = [&](size_t count, qreal itemHeight) -> qreal {
        return count == 0 ? 0.0 : (count - 1) * itemSpacing + itemHeight;
    };
    const qreal topExtra = numTop * kTrackSpacing;
    const qreal botExtra = numBot * kTrackSpacing;
    const qreal innerHeight = std::max({columnHeight(numInputs, m.portHeight), columnHeight(numGates, m.gateHeight),
                                        columnHeight(numOutputs, m.portHeight), m.portHeight}) +
                              (kColumnPadding * 2);

    const qreal moduleWidth = leftChannelWidth + m.gateWidth + rightChannelWidth;
    const qreal moduleHeight = innerHeight + topExtra + botExtra;
    const qreal moduleX = std::max(80.0, (m.viewSize.width() - moduleWidth) / 2.0);
    const qreal moduleY = std::max(20.0, (m.viewSize.height() - moduleHeight) / 2.0);
    const qreal innerY = moduleY + topExtra;

    const qreal gateLeftX = moduleX + leftChannelWidth;
    const qreal gateRightX = gateLeftX + m.gateWidth;
    const qreal moduleRightX = moduleX + moduleWidth;

    SchematicLayout layout;
    layout.moduleFrame = QRectF(moduleX, moduleY, moduleWidth, moduleHeight);
    layout.titlePos = QPointF(moduleX + 15, moduleY + 10);

    // Vertical start of a column of `count` items, centered inside the inner module area
    auto columnStartY = [&](size_t count, qreal itemHeight) -> qreal {
        return innerY + (innerHeight - columnHeight(count, itemHeight)) / 2.0;
    };

    const qreal gateStartY = columnStartY(numGates, m.gateHeight);
    auto gateTopY = [&](size_t g) -> qreal { return gateStartY + (g * itemSpacing); };
    auto gateInputPin = [&](size_t g, size_t idx) -> QPointF {
        // Distribute input pins evenly across gate height
        size_t totalPins = gates[g].inputWires.size();
        qreal pinOffset = totalPins == 1 ? (m.gateHeight / 2.0) : (m.gateHeight * (idx + 1) / (totalPins + 1.0));
        return QPointF(gateLeftX, gateTopY(g) + pinOffset);
    };
    auto gateOutputPin = [&](size_t g) -> QPointF {
        return QPointF(gateRightX, gateTopY(g) + (m.gateHeight / 2.0));
    };

    // ---- 3. Placement ----
    for (size_t g = 0; g < numGates; ++g) {
        layout.gates.push_back(QRectF(gateLeftX, gateTopY(g), m.gateWidth, m.gateHeight));
    }

    std::map<std::string, QPointF> inputPins, outputPins;
    const qreal inputStartY = columnStartY(numInputs, m.portHeight);
    for (size_t i = 0; i < numInputs; ++i) {
        const std::string& input = inputs[i];
        // Keep the input's stub off the heights of gate pins that belong to other nets
        std::vector<qreal> foreignPinYs;
        for (size_t g = 0; g < numGates; ++g) {
            for (size_t idx = 0; idx < gates[g].inputWires.size(); ++idx) {
                if (gates[g].inputWires[idx] != input) foreignPinYs.push_back(gateInputPin(g, idx).y());
            }
        }
        qreal top = inputStartY + (i * itemSpacing);
        top += nudgeOffset(top + m.portHeight / 2.0, foreignPinYs);

        QPointF pin(moduleX, top + m.portHeight / 2.0);
        layout.inputs.push_back({input, QRectF(moduleX - m.inputBoxWidth, top, m.inputBoxWidth, m.portHeight), pin});
        inputPins[input] = pin;
    }

    const qreal outStartY = columnStartY(numOutputs, m.portHeight);
    for (size_t i = 0; i < numOutputs; ++i) {
        const std::string& output = outputs[i];
        std::vector<qreal> foreignOutYs;
        for (size_t g = 0; g < numGates; ++g) {
            if (gates[g].outputWire != output) foreignOutYs.push_back(gateOutputPin(g).y());
        }
        qreal top = outStartY + (i * itemSpacing);
        top += nudgeOffset(top + m.portHeight / 2.0, foreignOutYs);

        QPointF pin(moduleRightX, top + m.portHeight / 2.0);
        layout.outputs.push_back({output, QRectF(moduleRightX, top, m.outputBoxWidth, m.portHeight), pin});
        outputPins[output] = pin;
    }

    // ---- 4. Channel-based orthogonal routing ----
    const qreal leftStep = leftChannelWidth / (leftTracks + 1.0);
    const qreal rightStep = rightChannelWidth / (rightTracks + 1.0);
    auto leftTrackX = [&](size_t k) -> qreal { return moduleX + (k + 1) * leftStep; };
    auto rightTrackX = [&](size_t k) -> qreal { return gateRightX + (k + 1) * rightStep; };

    // 4.1 Primary inputs -> gate inputs (outer part of the left channel)
    for (size_t k = 0; k < inputNets.size(); ++k) {
        const auto& net = inputNets[k];
        QPointF src = inputPins[net];
        qreal trackX = leftTrackX(k);

        WireRoute route{net, {}};
        for (const auto& pin : inputNetPins[net]) {
            QPointF dst = gateInputPin(pin.gate, pin.pin);
            // Horizontal to the net's track, vertical along it, horizontal into the pin
            route.branches.push_back({src, {trackX, src.y()}, {trackX, dst.y()}, dst});
        }
        layout.wires.push_back(std::move(route));
    }

    // 4.2 Gate-to-gate: right pin -> right corridor -> top/bottom gutter -> left corridor -> left pin
    if (!g2gNets.empty()) {
        const qreal topBypassBase = gateStartY - kBypassClearance;
        const qreal botBypassBase = gateTopY(numGates - 1) + m.gateHeight + kBypassClearance;

        for (size_t j = 0; j < g2gNets.size(); ++j) {
            const auto& net = g2gNets[j];
            QPointF src = gateOutputPin(driverGate[net]);
            qreal rightX = rightTrackX(j);
            qreal leftX = leftTrackX(inputNets.size() + j);
            size_t bypassIdx = g2gBypassIdx[net];
            qreal bypassY = g2gViaTop[net] ? (topBypassBase - bypassIdx * kTrackSpacing)
                                           : (botBypassBase + bypassIdx * kTrackSpacing);

            WireRoute route{net, {}};
            for (const auto& pin : g2gNetPins[net]) {
                QPointF dst = gateInputPin(pin.gate, pin.pin);
                route.branches.push_back({src, {rightX, src.y()}, {rightX, bypassY},
                                          {leftX, bypassY}, {leftX, dst.y()}, dst});
            }
            layout.wires.push_back(std::move(route));
        }
    }

    // 4.3 Gate outputs -> primary outputs (outer part of the right channel)
    for (size_t k = 0; k < outNets.size(); ++k) {
        const auto& net = outNets[k];
        QPointF src = gateOutputPin(driverGate[net]);
        QPointF dst = outputPins[net];
        qreal trackX = rightTrackX(g2gNets.size() + k);
        layout.wires.push_back({net, {{src, {trackX, src.y()}, {trackX, dst.y()}, dst}}});
    }

    return layout;
}

} // namespace MiniEDA
