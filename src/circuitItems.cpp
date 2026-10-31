#include "circuitItems.hpp"
#include <QBrush>
#include <QGraphicsSceneMouseEvent>

namespace MiniEDA {

namespace {

const qreal kLabelPad = 4.0; // Space between a label and its box border

QSizeF measure(const QString& text, const QFont& font) {
    QGraphicsTextItem probe(text);
    probe.setFont(font);
    return probe.boundingRect().size();
}

// Creates a label as a child of `box`, left-aligned and vertically centered in it
QGraphicsTextItem* addLabel(QGraphicsRectItem* box, const QString& text, const QFont& font = QFont()) {
    auto* label = new QGraphicsTextItem(text, box);
    label->setFont(font);
    QRectF rect = box->rect();
    label->setPos(rect.left() + kLabelPad, rect.top() + (rect.height() - label->boundingRect().height()) / 2.0);
    return label;
}

QString portLabel(const std::string& net, LogicState state) {
    return QString::fromStdString(net) + " (" + stateText(state) + ")";
}

} // namespace

// ---- Palette ----

namespace Palette {

QColor inputFill(LogicState state) {
    switch (state) {
        case LogicState::LogicHigh: return QColor(255, 100, 100); // Red
        case LogicState::LogicLow:  return QColor(Qt::lightGray);
        default:                    return QColor(255, 165, 0);   // Orange
    }
}

QColor outputFill(LogicState state) {
    switch (state) {
        case LogicState::LogicHigh: return QColor(144, 238, 144); // Green
        case LogicState::LogicLow:  return QColor(220, 220, 220); // Gray
        default:                    return QColor(255, 222, 173); // Orange
    }
}

QColor gateFill(LogicState state) {
    switch (state) {
        case LogicState::LogicHigh: return QColor(152, 251, 152); // Soft green
        case LogicState::LogicLow:  return QColor(230, 230, 230); // Gray
        default:                    return QColor(255, 235, 205); // Soft orange
    }
}

QPen wirePen(LogicState state) {
    switch (state) {
        case LogicState::LogicHigh: return QPen(QColor(255, 90, 90), 3);  // Solid red
        case LogicState::LogicLow:  return QPen(QColor(65, 105, 225), 3); // Solid blue
        default:                    return QPen(QColor(255, 165, 0), 2);  // Thin orange
    }
}

QPen gateBorder() { return QPen(Qt::darkBlue, 2); }
QPen portBorder() { return QPen(Qt::black, 2); }
QFont gateFont() { return QFont("Courier New", 9); }

} // namespace Palette

QString stateText(LogicState state) {
    switch (state) {
        case LogicState::LogicHigh: return "1";
        case LogicState::LogicLow:  return "0";
        default:                    return "Z";
    }
}

// ---- InputPortItem ----

InputPortItem::InputPortItem(const std::string& net, const QRectF& box) : QGraphicsRectItem(box), m_net(net) {
    setPen(Palette::portBorder());
    m_label = addLabel(this, portLabel(net, m_state));
    setState(m_state);
}

void InputPortItem::setState(LogicState state) {
    m_state = state;
    setBrush(Palette::inputFill(state));
    m_label->setPlainText(portLabel(m_net, state));
}

qreal InputPortItem::labelWidth(const std::string& net) {
    qreal width = 0;
    for (LogicState state : {LogicState::LogicLow, LogicState::LogicHigh, LogicState::HighImpedance}) {
        width = std::max(width, measure(portLabel(net, state), QFont()).width());
    }
    return width + 2 * kLabelPad;
}

void InputPortItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        setState(m_state == LogicState::LogicHigh ? LogicState::LogicLow : LogicState::LogicHigh);
        if (onToggled) onToggled(m_net, m_state);
    }
    // Base class ignores the press, so dragging from a port still pans the view
    QGraphicsRectItem::mousePressEvent(event);
}

// ---- OutputPortItem ----

OutputPortItem::OutputPortItem(const std::string& net, const QRectF& box) : QGraphicsRectItem(box), m_net(net) {
    setPen(Palette::portBorder());
    m_label = addLabel(this, portLabel(net, LogicState::HighImpedance));
    setState(LogicState::HighImpedance);
}

void OutputPortItem::setState(LogicState state) {
    setBrush(Palette::outputFill(state));
    m_label->setPlainText(portLabel(m_net, state));
}

// ---- GateItem ----

GateItem::GateItem(const Gate& gate, const QRectF& box) : QGraphicsRectItem(box), m_gate(gate) {
    setPen(Palette::gateBorder());
    setBrush(QColor(240, 248, 255));
    std::vector<LogicState> unknown(gate.inputWires.size(), LogicState::HighImpedance);
    m_label = addLabel(this, labelText(gate, unknown, LogicState::HighImpedance), Palette::gateFont());
}

void GateItem::setStates(const std::vector<LogicState>& inputs, LogicState output) {
    setBrush(Palette::gateFill(output));
    m_label->setPlainText(labelText(m_gate, inputs, output));
}

QSizeF GateItem::labelSize(const Gate& gate) {
    std::vector<LogicState> unknown(gate.inputWires.size(), LogicState::HighImpedance);
    QSizeF size = measure(labelText(gate, unknown, LogicState::HighImpedance), Palette::gateFont());
    return size + QSizeF(2 * kLabelPad, 2 * kLabelPad);
}

QString GateItem::labelText(const Gate& gate, const std::vector<LogicState>& inputs, LogicState output) {
    QStringList pins;
    for (LogicState state : inputs) pins << stateText(state);
    return QString(gateTypeName(gate.type)).toUpper() + ": " + QString::fromStdString(gate.name) + "\n" +
           "In:  [" + pins.join(", ") + "]\n" +
           "Out: [" + stateText(output) + "]";
}

} // namespace MiniEDA
