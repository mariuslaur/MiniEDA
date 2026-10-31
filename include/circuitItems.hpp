#pragma once
#include "netlist.hpp"
#include <QFont>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QPen>
#include <functional>
#include <string>
#include <vector>

namespace MiniEDA {

// All state colours in one place
namespace Palette {
QColor inputFill(LogicState state);
QColor outputFill(LogicState state);
QColor gateFill(LogicState state);
QPen wirePen(LogicState state);
QPen gateBorder();
QPen portBorder();
QFont gateFont();
// Schematic text sits on light boxes, so it stays black even when the system theme is dark
QColor textColor();
} // namespace Palette

// "0", "1" or "Z"
QString stateText(LogicState state);

// Primary input on the left edge of the module; a click toggles it 0 <-> 1
class InputPortItem : public QGraphicsRectItem {
public:
    InputPortItem(const std::string& net, const QRectF& box);

    const std::string& net() const { return m_net; }
    LogicState state() const { return m_state; }
    void setState(LogicState state);

    // Called after a click changed the state
    std::function<void(const std::string& net, LogicState state)> onToggled;

    // Width needed by the label in its widest state
    static qreal labelWidth(const std::string& net);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

private:
    std::string m_net;
    LogicState m_state = LogicState::LogicLow;
    QGraphicsTextItem* m_label;
};

// Primary output on the right edge of the module
class OutputPortItem : public QGraphicsRectItem {
public:
    OutputPortItem(const std::string& net, const QRectF& box);

    const std::string& net() const { return m_net; }
    void setState(LogicState state);

    static qreal labelWidth(const std::string& net) { return InputPortItem::labelWidth(net); }

private:
    std::string m_net;
    QGraphicsTextItem* m_label;
};

// Gate box showing its type, name and live pin values
class GateItem : public QGraphicsRectItem {
public:
    GateItem(const Gate& gate, const QRectF& box);

    void setStates(const std::vector<LogicState>& inputs, LogicState output);

    // Size of the label with every pin shown (all pin values have the same width)
    static QSizeF labelSize(const Gate& gate);

private:
    Gate m_gate;
    QGraphicsTextItem* m_label;

    static QString labelText(const Gate& gate, const std::vector<LogicState>& inputs, LogicState output);
};

} // namespace MiniEDA
