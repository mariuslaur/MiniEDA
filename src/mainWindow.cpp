#include "mainWindow.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QGraphicsTextItem>
#include <fstream>
#include <sstream>
#include <map>

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
    
    // Local tracking map for wire connection layout paths
    std::map<std::string, QPointF> pinPositions;

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

    size_t numInputs  = m_parser.getInputs().size();
    size_t numGates   = m_parser.getGates().size();
    size_t numOutputs = m_parser.getOutputs().size();

    // Max number of column elements will drive the module graphical height boundary rules
    size_t maxElements = std::max({numInputs, numGates, numOutputs});
    if (maxElements == 0) maxElements = 1;

    qreal moduleWidth = 480;
    qreal moduleHeight = (maxElements * itemSpacing) + (padding * 2) - 30;

    qreal viewWidth = view->width() > 0 ? view->width() : 1000;
    qreal viewHeight = view->height() > 0 ? view->height() : 600;

    qreal moduleX = (viewWidth - moduleWidth) / 2.0;
    qreal moduleY = std::max(20.0, (viewHeight - moduleHeight) / 2.0);

    scene->setSceneRect(0, 0, viewWidth, std::max(viewHeight, moduleY + moduleHeight + 50));

    // Adaptive structural frame background module rendering
    scene->addRect(moduleX, moduleY, moduleWidth, moduleHeight, 
                    QPen(QColor(80, 80, 80), 2, Qt::DashLine), QBrush(QColor(253, 253, 253)));
    
    auto *moduleTitle = scene->addText("Module: TopLevelNetlist (Adaptive Internal View)");
    moduleTitle->setFont(QFont("Arial", 11, QFont::Bold));
    moduleTitle->setPos(moduleX + 15, moduleY + 10);

    // Interactive module inputs generation loop (centered along the left side boundary)
    qreal inputStartY = moduleY + padding + ((moduleHeight - (padding * 2) - ((numInputs - 1) * itemSpacing)) / 2.0);
    if (numInputs <= 1) inputStartY = moduleY + (moduleHeight / 2.0) - 15;

    for (size_t i = 0; i < numInputs; ++i) {
        std::string input = m_parser.getInputs().at(i);
        
        // Initialize default high-impedance state into the logic engine securely
        m_parser.setInputState(input, MiniEDA::LogicState::HighImpedance);

        qreal currY = inputStartY + (i * itemSpacing);

        auto *inputItem = new InteractiveInputItem(input, moduleX - 60, currY);
        inputItem->setData(3, "INPUT");
        scene->addItem(inputItem);

        auto *text = scene->addText(QString::fromStdString(input + " (Z)"));
        text->setPos(moduleX - 55, currY + 2);

        pinPositions[input] = QPointF(moduleX, currY + 15);

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

    // Internal structural logic gates placement loops (centered inside the module canvas frame)
    qreal gateStartY = moduleY + padding + ((moduleHeight - (padding * 2) - ((numGates - 1) * itemSpacing)) / 2.0);
    if (numGates <= 1) gateStartY = moduleY + (moduleHeight / 2.0) - (gateHeight / 2.0);

    for (size_t i = 0; i < numGates; ++i) {
        const auto& gate = m_parser.getGates().at(i);
        std::string gateTypeStr = gateTypeToString(gate.type);
        qreal currY = gateStartY + (i * itemSpacing);

        auto *gateRect = scene->addRect(moduleX + 150, currY, gateWidth, gateHeight, QPen(Qt::darkBlue, 2), QBrush(QColor(240, 248, 255)));
        
        gateRect->setData(0, QString::fromStdString(gate.name));
        gateRect->setData(1, QString::fromStdString(gateTypeStr));
        gateRect->setData(2, QString::fromStdString(gate.outputWire));
        gateRect->setData(3, "GATE");

        auto *gateText = scene->addText(QString::fromStdString(gateTypeStr + ": " + gate.name + "\nIn: [Z, Z]\nOut: Z"));
        gateText->setFont(QFont("Courier New", 9));
        gateText->setPos(moduleX + 155, currY + 5);
        gateText->setParentItem(gateRect);

        // Capture component physical output pin mapping position details safely
        pinPositions[gate.outputWire] = QPointF(moduleX + 150 + gateWidth, currY + (gateHeight / 2.0));
    }

    // Module structural outputs generation loop (centered along the right side boundary)
    qreal outStartY = moduleY + padding + ((moduleHeight - (padding * 2) - ((numOutputs - 1) * itemSpacing)) / 2.0);
    if (numOutputs <= 1) outStartY = moduleY + (moduleHeight / 2.0) - 15;

    for (size_t i = 0; i < numOutputs; ++i) {
        std::string output = m_parser.getOutputs().at(i);
        qreal currY = outStartY + (i * itemSpacing);

        auto *outItem = new VisualOutputItem(output, moduleX + moduleWidth, currY);
        outItem->setData(0, QString::fromStdString(output));
        outItem->setData(3, "OUTPUT");
        scene->addItem(outItem);

        auto *outText = scene->addText(QString::fromStdString(output + " (Z)"));
        outText->setPos(moduleX + moduleWidth + 5, currY + 2);

        pinPositions["OUT_" + output] = QPointF(moduleX + moduleWidth, currY + 15);
    }

    // Inter-wire structural trace routing generation loops
    for (size_t i = 0; i < numGates; ++i) {
        const auto& gate = m_parser.getGates().at(i);
        qreal currY = gateStartY + (i * itemSpacing);

        int idx = 0;
        for (const auto& inputWire : gate.inputWires) {
            if (pinPositions.count(inputWire)) {
                QPointF startPoint = pinPositions[inputWire];
                // Input pin wire geometric routing offsets mapping
                QPointF endPoint(moduleX + 150, currY + (idx == 0 ? 18 : 48));

                auto *lineItem = scene->addLine(QLineF(startPoint, endPoint), QPen(QColor(255, 165, 0), 2));
                lineItem->setData(0, QString::fromStdString(inputWire));
            }
            idx++;
        }

        if (pinPositions.count("OUT_" + gate.outputWire)) {
            QPointF startPoint = pinPositions[gate.outputWire];
            QPointF endPoint = pinPositions["OUT_" + gate.outputWire];

            auto *lineItem = scene->addLine(QLineF(startPoint, endPoint), QPen(QColor(255, 165, 0), 2));
            lineItem->setData(0, QString::fromStdString(gate.outputWire));
        }
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
        if (item->type() == QGraphicsLineItem::Type && !item->data(0).toString().isEmpty()) {
            std::string wireName = item->data(0).toString().toStdString();
            
            // Defensively pull the state from the memory graph using safe map queries
            MiniEDA::LogicState val = m_parser.getWireState(wireName);
            auto* line = static_cast<QGraphicsLineItem*>(item);
            
            if (val == MiniEDA::LogicState::LogicHigh) {
                line->setPen(QPen(QColor(255, 90, 90), 3));   // (1) Solid Red
            } else if (val == MiniEDA::LogicState::LogicLow) {
                line->setPen(QPen(QColor(65, 105, 225), 3));  // (0) Solid Blue
            } else {
                line->setPen(QPen(QColor(255, 165, 0), 2));   // (Z) Thin High-Impedance Orange
            }
        }

        // 2. Update the internal logical gate boxes text blocks and background tints
        if (item->type() == QGraphicsRectItem::Type && item->data(3).toString() == "GATE") {
            QString gName = item->data(0).toString();
            QString gType = item->data(1).toString();
            QString gOutput = item->data(2).toString();

            MiniEDA::LogicState p1 = MiniEDA::LogicState::HighImpedance;
            MiniEDA::LogicState p2 = MiniEDA::LogicState::HighImpedance;

            // Search for current matching component configurations to correctly display inputs
            for (const auto& gate : m_parser.getGates()) {
                if (gate.name == gName.toStdString()) {
                    if (gate.inputWires.size() > 0) {
                        p1 = m_parser.getWireState(gate.inputWires.at(0));
                    }
                    if (gate.inputWires.size() > 1) {
                        p2 = m_parser.getWireState(gate.inputWires.at(1));
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
                                         "In:  [" + QString::fromStdString(formatPinState(p1)) + ", " + QString::fromStdString(formatPinState(p2)) + "]\n" +
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

            // Sync structural label texts rendered inside the viewport
            for (auto* sibling : scene->items()) {
                auto* textItem = qgraphicsitem_cast<QGraphicsTextItem*>(sibling);
                if (textItem && textItem->parentItem() == nullptr && 
                    textItem->toPlainText().startsWith(QString::fromStdString(outName))) {
                    QString status = QString::fromStdString(outName) + " (" + QString::fromStdString(formatPinState(finalOut)) + ")";
                    textItem->setPlainText(status);
                }
            }
        }
    }
}