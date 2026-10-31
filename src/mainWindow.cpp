#include "mainWindow.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QGraphicsTextItem>
#include <QPainterPath>
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <cmath>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    resize(1100, 650);

    auto *mainLayout = new QHBoxLayout(centralWidget);
    auto *leftLayout = new QVBoxLayout();

    editor = new QTextEdit(this);
    editor->setPlaceholderText("// Write here or open file with verilog code...");

    auto *btnOpen = new QPushButton("Open File", this);
    auto *btnSimulate = new QPushButton("Simulate", this);

    leftLayout->addWidget(editor);
    leftLayout->addWidget(btnOpen);
    leftLayout->addWidget(btnSimulate);

    scene = new QGraphicsScene(this);
    view = new QGraphicsView(scene, this);
    view->setRenderHint(QPainter::Antialiasing);

    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addWidget(view, 2);

    connect(btnOpen, &QPushButton::clicked, this, &MainWindow::openFile);

    connect(btnSimulate, &QPushButton::clicked, this, [this]() {
        // Extract raw string directly from the UI editor to avoid slow disk I/O operations
        std::string verilogCode = this->editor->toPlainText().toStdString();

        try {
            // Parse the in-memory string directly using our optimized defensive engine
            this->m_parser.parseNetlist(verilogCode);

            if (this->m_parser.getGates().empty()) {
                QMessageBox::warning(this, "Simulation Notice", "The syntax is valid, but no structural logic gates were discovered to simulate.");
                return;
            }

            // Propagate signals across the freshly loaded structural logic topology
            this->m_parser.evaluateCircuit();

            // Render the graphical scene mapping using viewport geometry recalculations
            this->renderCircuit();

        } catch (const MiniEDA::SyntaxException& ex) {
            // Catch precision syntax coordinates and feed them back directly into the UI layer
            QString errorMsg = QString("Syntax Error: %1\nLine: %2, Column: %3")
                               .arg(QString::fromStdString(ex.rawMessage()))
                               .arg(ex.line())
                               .arg(ex.column());
            QMessageBox::critical(this, "Parser Failure", errorMsg);
        } catch (const std::exception& ex) {
            // General fallback architecture mapping to prevent crashes on out_of_bounds violations
            QMessageBox::critical(this, "Fatal Error", QString::fromStdString(ex.what()));
        }
    });
}

MainWindow::~MainWindow() {
    // Left blank intentionally.
    // Qt's parent-child object tree automatically deletes all child widgets,
    // and m_parser is automatically cleaned up as a stack-allocated member.
}

void MainWindow::openFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open Verilog Netlist", "", "Verilog Files (*.v);;Text Files (*.txt)");
    if (fileName.isEmpty()) return;

    std::ifstream file(fileName.toStdString());
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        editor->setText(QString::fromStdString(buffer.str()));
        file.close();

        try {
            // Directly load and process the opened file buffer inside the parser instance
            m_parser.parseNetlist(buffer.str());
            m_parser.evaluateCircuit();
            renderCircuit();
        } catch (const MiniEDA::SyntaxException& ex) {
            QString errorMsg = QString("Syntax Error in loaded file: %1\nLine: %2, Column: %3")
                               .arg(QString::fromStdString(ex.rawMessage()))
                               .arg(ex.line())
                               .arg(ex.column());
            QMessageBox::critical(this, "Parser Failure", errorMsg);
        }
    }
}

void MainWindow::renderCircuit() {
    scene->clear();

    // Helper lambda to map the modern strongly-typed GateType enum back to raw UI display strings
    auto gateTypeToString = [](MiniEDA::GateType type) -> std::string {
        switch (type) {
            case MiniEDA::GateType::AND:  return "and";
            case MiniEDA::GateType::OR:   return "or";
            case MiniEDA::GateType::XOR:  return "xor";
            case MiniEDA::GateType::NAND: return "nand";
            case MiniEDA::GateType::NOR:  return "nor";
            case MiniEDA::GateType::XNOR: return "xnor";
            case MiniEDA::GateType::BUF:  return "buf";
            case MiniEDA::GateType::NOT:  return "not";
        }
        return "unknown";
    };

    // Dynamic detection of module graphical dimensions using modern encapsulation layers
    const int itemSpacing = 80;
    const int padding = 60;
    const int gateWidth = 180;
    const int gateHeight = 65;
    const qreal trackSpacing = 14.0;      // Minimal distance between two parallel wire tracks
    const qreal minChannelWidth = 150.0;  // Routing channel width when only a few tracks are needed

    const auto& inputs  = m_parser.getInputs();
    const auto& gates   = m_parser.getGates();
    const auto& outputs = m_parser.getOutputs();

    size_t numInputs  = inputs.size();
    size_t numGates   = gates.size();
    size_t numOutputs = outputs.size();

    // ---- 1. Net topology classification (independent of geometry) ----
    // Each net gets exactly one vertical track per channel, so different nets can never overlap.
    struct PinRef { size_t gate; size_t pin; };

    std::map<std::string, size_t> driverGate; // wire -> index of the gate driving it
    for (size_t g = 0; g < numGates; ++g) driverGate[gates[g].outputWire] = g;
    std::set<std::string> inputSet(inputs.begin(), inputs.end());

    std::map<std::string, std::vector<PinRef>> inputNetPins; // module input -> gate input pins
    std::map<std::string, std::vector<PinRef>> g2gNetPins;   // gate output  -> gate input pins
    for (size_t g = 0; g < numGates; ++g) {
        for (size_t idx = 0; idx < gates[g].inputWires.size(); ++idx) {
            const auto& wire = gates[g].inputWires[idx];
            if (driverGate.count(wire)) g2gNetPins[wire].push_back({g, idx});
            else if (inputSet.count(wire)) inputNetPins[wire].push_back({g, idx});
        }
    }

    // Track order: inputs top-to-bottom, gate nets by driver gate, outputs top-to-bottom
    std::vector<std::string> inputNets, g2gNets, outNets;
    for (const auto& in : inputs) if (inputNetPins.count(in)) inputNets.push_back(in);
    for (const auto& gate : gates) if (g2gNetPins.count(gate.outputWire)) g2gNets.push_back(gate.outputWire);
    for (const auto& out : outputs) if (driverGate.count(out)) outNets.push_back(out);

    // Gate-to-gate nets bypass the gate column over the top or under the bottom, each on its own gutter
    qreal midGate = (static_cast<qreal>(numGates) - 1.0) / 2.0;
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
    size_t leftTracks  = inputNets.size() + g2gNets.size();
    size_t rightTracks = g2gNets.size() + outNets.size();
    qreal leftChannelWidth  = std::max(minChannelWidth, (leftTracks + 1) * trackSpacing);
    qreal rightChannelWidth = std::max(minChannelWidth, (rightTracks + 1) * trackSpacing);

    // Max number of column elements will drive the module graphical height boundary rules
    size_t maxElements = std::max({numInputs, numGates, numOutputs});
    if (maxElements == 0) maxElements = 1;

    qreal topExtra = numTop * trackSpacing;
    qreal botExtra = numBot * trackSpacing;
    qreal innerHeight = (maxElements * itemSpacing) + (padding * 2) - 30;

    qreal moduleWidth = leftChannelWidth + gateWidth + rightChannelWidth;
    qreal moduleHeight = innerHeight + topExtra + botExtra;

    qreal viewWidth = view->width() > 0 ? view->width() : 1000;
    qreal viewHeight = view->height() > 0 ? view->height() : 600;

    qreal moduleX = std::max(80.0, (viewWidth - moduleWidth) / 2.0);
    qreal moduleY = std::max(20.0, (viewHeight - moduleHeight) / 2.0);
    qreal innerY = moduleY + topExtra;

    qreal gateLeftX = moduleX + leftChannelWidth;
    qreal gateRightX = gateLeftX + gateWidth;
    qreal moduleRightX = moduleX + moduleWidth;

    scene->setSceneRect(0, 0, std::max(viewWidth, moduleRightX + 120), std::max(viewHeight, moduleY + moduleHeight + 50));

    // Adaptive structural frame background module rendering
    scene->addRect(moduleX, moduleY, moduleWidth, moduleHeight,
                    QPen(QColor(80, 80, 80), 2, Qt::DashLine), QBrush(QColor(253, 253, 253)));

    auto *moduleTitle = scene->addText("Module: TopLevelNetlist (Adaptive Internal View)");
    moduleTitle->setFont(QFont("Arial", 11, QFont::Bold));
    moduleTitle->setPos(moduleX + 15, moduleY + 10);

    // Vertical start of a column of `count` items, centered inside the inner module area
    auto columnStartY = [&](size_t count, qreal itemHeight) -> qreal {
        if (count <= 1) return innerY + (innerHeight / 2.0) - (itemHeight / 2.0);
        return innerY + padding + ((innerHeight - (padding * 2) - ((count - 1) * itemSpacing)) / 2.0);
    };

    qreal gateStartY = columnStartY(numGates, gateHeight);
    auto gateTopY = [&](size_t g) -> qreal { return gateStartY + (g * itemSpacing); };
    auto gateInputPin = [&](size_t g, size_t idx) -> QPointF {
        // Distribute input pins evenly across gate height
        size_t totalPins = gates[g].inputWires.size();
        qreal pinOffset = totalPins == 1 ? (gateHeight / 2.0) : (gateHeight * (idx + 1) / (totalPins + 1.0));
        return QPointF(gateLeftX, gateTopY(g) + pinOffset);
    };
    auto gateOutputPin = [&](size_t g) -> QPointF {
        return QPointF(gateRightX, gateTopY(g) + (gateHeight / 2.0));
    };

    // Shifts a module port slightly when its horizontal stub would be collinear with a stub of another net
    auto nudgeOffset = [](qreal pinY, const std::vector<qreal>& foreignYs) -> qreal {
        for (qreal offset : {0.0, 10.0, -10.0, 20.0, -20.0}) {
            bool clash = false;
            for (qreal y : foreignYs) {
                if (std::abs(pinY + offset - y) < 4.0) { clash = true; break; }
            }
            if (!clash) return offset;
        }
        return 0.0;
    };

    // ---- 3. Interactive module inputs (centered along the left side boundary) ----
    std::map<std::string, qreal> inputPinY;
    qreal inputStartY = columnStartY(numInputs, 30);

    for (size_t i = 0; i < numInputs; ++i) {
        std::string input = inputs.at(i);

        // Initialize default high-impedance state into the logic engine securely
        m_parser.setInputState(input, MiniEDA::LogicState::HighImpedance);

        std::vector<qreal> foreignPinYs;
        for (size_t g = 0; g < numGates; ++g) {
            for (size_t idx = 0; idx < gates[g].inputWires.size(); ++idx) {
                if (gates[g].inputWires[idx] != input) foreignPinYs.push_back(gateInputPin(g, idx).y());
            }
        }

        qreal currY = inputStartY + (i * itemSpacing);
        currY += nudgeOffset(currY + 15, foreignPinYs);

        auto *inputItem = new InteractiveInputItem(input, moduleX - 60, currY);
        inputItem->setData(3, "INPUT");
        scene->addItem(inputItem);

        auto *text = scene->addText(QString::fromStdString(input + " (Z)"));
        text->setData(0, QString::fromStdString(input));
        text->setData(3, "INPUT_LABEL");
        text->setPos(moduleX - 55, currY + 2);

        inputPinY[input] = currY + 15;

        // UI trigger event mapping input item values back to the core tri-state logic engine
        inputItem->onClickCallback = [this, inputItem, text, input]() {
            MiniEDA::LogicState state = MiniEDA::LogicState::HighImpedance;
            QString stateStr = " (Z)";

            if (inputItem->value == 1) {
                state = MiniEDA::LogicState::LogicHigh;
                stateStr = " (1)";
            } else if (inputItem->value == 0) {
                state = MiniEDA::LogicState::LogicLow;
                stateStr = " (0)";
            }

            this->m_parser.setInputState(input, state);
            text->setPlainText(QString::fromStdString(input) + stateStr);

            // Re-run the global structural bitwise logic propagation layers
            this->runSimulation();
        };
    }

    // ---- 4. Internal structural logic gates (centered inside the module canvas frame) ----
    for (size_t i = 0; i < numGates; ++i) {
        const auto& gate = gates.at(i);
        std::string gateTypeStr = gateTypeToString(gate.type);
        qreal currY = gateTopY(i);

        auto *gateRect = scene->addRect(gateLeftX, currY, gateWidth, gateHeight, QPen(Qt::darkBlue, 2), QBrush(QColor(240, 248, 255)));

        gateRect->setData(0, QString::fromStdString(gate.name));
        gateRect->setData(1, QString::fromStdString(gateTypeStr));
        gateRect->setData(2, QString::fromStdString(gate.outputWire));
        gateRect->setData(3, "GATE");

        std::string pinsZ;
        for (size_t p = 0; p < gate.inputWires.size(); ++p) pinsZ += (p ? ", Z" : "Z");
        auto *gateText = scene->addText(QString::fromStdString(gateTypeStr + ": " + gate.name + "\nIn: [" + pinsZ + "]\nOut: Z"));
        gateText->setFont(QFont("Courier New", 9));
        gateText->setPos(gateLeftX + 5, currY + 5);
        gateText->setParentItem(gateRect);
    }

    // ---- 5. Module structural outputs (centered along the right side boundary) ----
    std::map<std::string, qreal> outputPinY;
    qreal outStartY = columnStartY(numOutputs, 30);

    for (size_t i = 0; i < numOutputs; ++i) {
        std::string output = outputs.at(i);

        std::vector<qreal> foreignOutYs;
        for (size_t g = 0; g < numGates; ++g) {
            if (gates[g].outputWire != output) foreignOutYs.push_back(gateOutputPin(g).y());
        }

        qreal currY = outStartY + (i * itemSpacing);
        currY += nudgeOffset(currY + 15, foreignOutYs);

        auto *outItem = new VisualOutputItem(output, moduleRightX, currY);
        outItem->setData(0, QString::fromStdString(output));
        outItem->setData(3, "OUTPUT");
        scene->addItem(outItem);

        auto *outText = scene->addText(QString::fromStdString(output + " (Z)"));
        outText->setData(0, QString::fromStdString(output));
        outText->setData(3, "OUTPUT_LABEL");
        outText->setPos(moduleRightX + 5, currY + 2);

        outputPinY[output] = currY + 15;
    }

    // ---- 6. Channel-based orthogonal routing: one dedicated track per net, no gate crossings ----
    qreal leftStep  = leftChannelWidth / (leftTracks + 1.0);
    qreal rightStep = rightChannelWidth / (rightTracks + 1.0);
    auto leftTrackX  = [&](size_t k) -> qreal { return moduleX + (k + 1) * leftStep; };
    auto rightTrackX = [&](size_t k) -> qreal { return gateRightX + (k + 1) * rightStep; };

    auto addNetPath = [this](const QPainterPath& path, const std::string& net) {
        auto *pathItem = scene->addPath(path, QPen(QColor(255, 165, 0), 2));
        pathItem->setData(0, QString::fromStdString(net));
    };

    // 6.1 Primary inputs -> gate inputs (outer part of the left channel)
    for (size_t k = 0; k < inputNets.size(); ++k) {
        const auto& net = inputNets[k];
        QPointF src(moduleX, inputPinY[net]);
        qreal trackX = leftTrackX(k);

        QPainterPath path;
        for (const auto& pin : inputNetPins[net]) {
            QPointF dst = gateInputPin(pin.gate, pin.pin);
            path.moveTo(src);
            path.lineTo(trackX, src.y()); // 1. Horizontal from source to the net's track
            path.lineTo(trackX, dst.y()); // 2. Vertical along the dedicated track
            path.lineTo(dst);             // 3. Horizontal to destination pin
        }
        addNetPath(path, net);
    }

    // 6.2 Gate-to-gate connections: right pin -> right corridor -> top/bottom gutter -> left corridor -> left pin
    if (!g2gNets.empty()) {
        qreal topBypassBase = gateStartY - 25.0;
        qreal botBypassBase = gateTopY(numGates - 1) + gateHeight + 25.0;

        for (size_t j = 0; j < g2gNets.size(); ++j) {
            const auto& net = g2gNets[j];
            QPointF src = gateOutputPin(driverGate[net]);
            qreal rightX = rightTrackX(j);
            qreal leftX  = leftTrackX(inputNets.size() + j);
            size_t bypassIdx = g2gBypassIdx[net];
            qreal bypassY = g2gViaTop[net] ? (topBypassBase - bypassIdx * trackSpacing)
                                           : (botBypassBase + bypassIdx * trackSpacing);

            QPainterPath path;
            for (const auto& pin : g2gNetPins[net]) {
                QPointF dst = gateInputPin(pin.gate, pin.pin);
                path.moveTo(src);
                path.lineTo(rightX, src.y()); // Step 1: Exit gate rightwards into right corridor
                path.lineTo(rightX, bypassY); // Step 2: Traverse vertically up/down outside gate bounds
                path.lineTo(leftX, bypassY);  // Step 3: Bypass horizontally above/below all gates
                path.lineTo(leftX, dst.y());  // Step 4: Traverse vertically in left corridor
                path.lineTo(dst);             // Step 5: Enter destination gate input pin from the left
            }
            addNetPath(path, net);
        }
    }

    // 6.3 Gate outputs -> primary outputs (outer part of the right channel)
    for (size_t k = 0; k < outNets.size(); ++k) {
        const auto& net = outNets[k];
        QPointF src = gateOutputPin(driverGate[net]);
        QPointF dst(moduleRightX, outputPinY[net]);
        qreal trackX = rightTrackX(g2gNets.size() + k);

        QPainterPath path;
        path.moveTo(src);
        path.lineTo(trackX, src.y());
        path.lineTo(trackX, dst.y());
        path.lineTo(dst);
        addNetPath(path, net);
    }

    // Trigger state evaluation and update UI visual nodes values mapping
    this->runSimulation();
}

void MainWindow::runSimulation() {
    // Execute backend structural tri-state logic propagation across the memory graph
    m_parser.evaluateCircuit();

    // Helper lambda to cleanly convert core enum logic states to user-friendly UI display strings
    auto formatPinState = [](MiniEDA::LogicState state) -> std::string {
        if (state == MiniEDA::LogicState::LogicHigh) return "1";
        if (state == MiniEDA::LogicState::LogicLow)  return "0";
        return "Z";
    };

    // Update the graphical objects dynamically based on the newly calculated backend states
    QList<QGraphicsItem*> itemsList = scene->items();
    for (auto* item : itemsList) {
        // 1. Trace wire lines color and thickness updates
        if (item->type() == QGraphicsPathItem::Type && !item->data(0).toString().isEmpty()) {
            std::string wireName = item->data(0).toString().toStdString();

            // Defensively pull the state from the memory graph using safe map queries
            MiniEDA::LogicState val = m_parser.getWireState(wireName);
            auto* pathItem = static_cast<QGraphicsPathItem*>(item);

            if (val == MiniEDA::LogicState::LogicHigh) {
                pathItem->setPen(QPen(QColor(255, 90, 90), 3));   // (1) Solid Red
            } else if (val == MiniEDA::LogicState::LogicLow) {
                pathItem->setPen(QPen(QColor(65, 105, 225), 3));  // (0) Solid Blue
            } else {
                pathItem->setPen(QPen(QColor(255, 165, 0), 2));   // (Z) Thin High-Impedance Orange
            }
        }

        // 2. Update the internal logical gate boxes text blocks and background tints
        if (item->type() == QGraphicsRectItem::Type && item->data(3).toString() == "GATE") {
            QString gName = item->data(0).toString();
            QString gType = item->data(1).toString();
            QString gOutput = item->data(2).toString();

            // Search for current matching component configurations to correctly display all inputs
            QString pinStates;
            for (const auto& gate : m_parser.getGates()) {
                if (gate.name == gName.toStdString()) {
                    for (size_t p = 0; p < gate.inputWires.size(); ++p) {
                        if (p > 0) pinStates += ", ";
                        pinStates += QString::fromStdString(formatPinState(m_parser.getWireState(gate.inputWires.at(p))));
                    }
                    break;
                }
            }

            MiniEDA::LogicState finalOut = m_parser.getWireState(gOutput.toStdString());
            auto* rect = static_cast<QGraphicsRectItem*>(item);

            if (finalOut == MiniEDA::LogicState::LogicHigh) {
                rect->setBrush(QBrush(QColor(152, 251, 152))); // (1) High-state Soft Green
            } else if (finalOut == MiniEDA::LogicState::LogicLow) {
                rect->setBrush(QBrush(QColor(230, 230, 230))); // (0) Low-state Standard Gray
            } else {
                rect->setBrush(QBrush(QColor(255, 235, 205))); // (Z) Undefined/Z Soft Orange
            }

            auto children = rect->childItems();
            if (!children.isEmpty()) {
                auto* textItem = qgraphicsitem_cast<QGraphicsTextItem*>(children.first());
                if (textItem) {
                    QString statusText = gType.toUpper() + ": " + gName + "\n" +
                                         "In:  [" + pinStates + "]\n" +
                                         "Out: [" + QString::fromStdString(formatPinState(finalOut)) + "]";
                    textItem->setPlainText(statusText);
                }
            }
        }

        // 3. Update top-level module visual output blocks
        if (item->type() == QGraphicsRectItem::Type && item->data(3).toString() == "OUTPUT") {
            std::string outName = item->data(0).toString().toStdString();
            MiniEDA::LogicState finalOut = m_parser.getWireState(outName);

            auto* rect = static_cast<QGraphicsRectItem*>(item);
            if (finalOut == MiniEDA::LogicState::LogicHigh) {
                rect->setBrush(QBrush(QColor(144, 238, 144))); // (1) Green
            } else if (finalOut == MiniEDA::LogicState::LogicLow) {
                rect->setBrush(QBrush(QColor(220, 220, 220))); // (0) Gray
            } else {
                rect->setBrush(QBrush(QColor(255, 222, 173))); // (Z) Orange
            }

            // Sync the label belonging to exactly this output (matched by tag, not by text prefix)
            for (auto* sibling : scene->items()) {
                if (sibling->data(3).toString() != "OUTPUT_LABEL" ||
                    sibling->data(0).toString().toStdString() != outName) continue;

                auto* textItem = qgraphicsitem_cast<QGraphicsTextItem*>(sibling);
                if (textItem) {
                    QString status = QString::fromStdString(outName) + " (" + QString::fromStdString(formatPinState(finalOut)) + ")";
                    textItem->setPlainText(status);
                }
            }
        }
    }
}
