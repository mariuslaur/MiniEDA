#pragma once
#include "netlist.hpp"
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <string>
#include <vector>

namespace MiniEDA {

// Box sizes depend on label text, so the view measures them; the layout itself is pure geometry
struct LayoutMetrics {
    qreal inputBoxWidth = 60;
    qreal outputBoxWidth = 60;
    qreal gateWidth = 180;
    qreal gateHeight = 65;
    qreal portHeight = 30;
    QSizeF viewSize{1000, 600}; // The module frame is centered in this area when it fits
};

struct PortPlacement {
    std::string net;
    QRectF box;
    QPointF pin; // Where the wire attaches (on the module frame)
};

// One drawn wire of a net. A net can have several (gate-to-gate and gate-to-output).
struct WireRoute {
    std::string net;
    std::vector<std::vector<QPointF>> branches; // Orthogonal polylines from the source to each destination pin
};

struct SchematicLayout {
    QRectF moduleFrame;
    QPointF titlePos;
    std::vector<PortPlacement> inputs;  // Same order as Netlist::inputs
    std::vector<PortPlacement> outputs; // Same order as Netlist::outputs
    std::vector<QRectF> gates;          // Same order as Netlist::gates
    std::vector<WireRoute> wires;
};

// Places inputs (left), gates (middle column) and outputs (right) inside a module frame and routes
// every connection orthogonally. Each net gets a dedicated vertical track per routing channel and
// gate-to-gate nets bypass the gate column on their own horizontal gutter, so wires of different
// nets never overlap and no wire crosses a gate.
SchematicLayout computeSchematicLayout(const Netlist& netlist, const LayoutMetrics& metrics);

} // namespace MiniEDA
