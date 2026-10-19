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
    editor->setPlaceholderText("// Scrie sau încarcă cod Verilog aici...");
    
    auto *btnOpen = new QPushButton("Deschide Fișier", this);
    auto *btnSimulate = new QPushButton("Simulează Circuit", this);
    
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
        // 1. Save the edited text in a temp file.
        std::ofstream tempFile("temp_circuit.v");
        tempFile << this->editor->toPlainText().toStdString();
        tempFile.close();

        // 2. Perse the new text.
        this->currentCircuit = VerilogParser::parseFile("temp_circuit.v");
        
        if (this->currentCircuit.gates.empty()) {
            QMessageBox::warning(this, "Eroare", "Circuitul modificat nu are porți logice active sau sintaxa este greșită.");
            return;
        }

        // 3. Update the geometry.
        this->renderCircuit(); 
    });
}

void MainWindow::openFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Deschide Verilog Netlist", "", "Verilog Files (*.v);;Text Files (*.txt)");
    if (fileName.isEmpty()) return;

    std::ifstream file(fileName.toStdString());
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        editor->setText(QString::fromStdString(buffer.str()));
        file.close();
        
        currentCircuit = VerilogParser::parseFile(fileName.toStdString());
        renderCircuit();
    }
}

void MainWindow::renderCircuit() {
    scene->clear();
    inputStates.clear();
    
    std::map<std::string, QPointF> pinPositions;

    // Dynamic detection of module graphical dimensions.
    const int itemSpacing = 80;
    const int padding = 60;
    const int gateWidth = 180;
    const int gateHeight = 65;

    size_t numInputs = currentCircuit.inputs.size();
    size_t numGates  = currentCircuit.gates.size();
    size_t numOutputs = currentCircuit.outputs.size();

    // Max number of column elements will drive the module graphical height.
    size_t maxElements = std::max({numInputs, numGates, numOutputs});
    if (maxElements == 0) maxElements = 1;

    qreal moduleWidth = 480;
    qreal moduleHeight = (maxElements * itemSpacing) + (padding * 2) - 30;

    qreal viewWidth = view->width() > 0 ? view->width() : 1000;
    qreal viewHeight = view->height() > 0 ? view->height() : 600;

    qreal moduleX = (viewWidth - moduleWidth) / 2.0;
    qreal moduleY = std::max(20.0, (viewHeight - moduleHeight) / 2.0);

    scene->setSceneRect(0, 0, viewWidth, std::max(viewHeight, moduleY + moduleHeight + 50));

    // Adaptive module.
    scene->addRect(moduleX, moduleY, moduleWidth, moduleHeight, 
                    QPen(QColor(80, 80, 80), 2, Qt::DashLine), QBrush(QColor(253, 253, 253)));
    
    auto *moduleTitle = scene->addText("Module: TopLevelNetlist (Adaptive Internal View)");
    moduleTitle->setFont(QFont("Arial", 11, QFont::Bold));
    moduleTitle->setPos(moduleX + 15, moduleY + 10);

    // Interractive inputs (centered on the left side).
    qreal inputStartY = moduleY + padding + ((moduleHeight - (padding * 2) - ((numInputs - 1) * itemSpacing)) / 2.0);
    if (numInputs <= 1) inputStartY = moduleY + (moduleHeight / 2.0) - 15;

    for (size_t i = 0; i < numInputs; ++i) {
        std::string input = currentCircuit.inputs.at(i);
        inputStates[input] = -1; // Default z-state.

        qreal currY = inputStartY + (i * itemSpacing);

        auto *inputItem = new InteractiveInputItem(input, moduleX - 60, currY);
        inputItem->setData(3, "INPUT");
        scene->addItem(inputItem);

        auto *text = scene->addText(QString::fromStdString(input + " (Z)"));
        text->setPos(moduleX - 55, currY + 2);

        pinPositions[input] = QPointF(moduleX, currY + 15);

        inputItem->onClickCallback = [this, inputItem, text, input]() {
            this->inputStates[input] = inputItem->value;
            QString txt = QString::fromStdString(input) + (inputItem->value == 1 ? " (1)" : (inputItem->value == 0 ? " (0)" : " (Z)"));
            text->setPlainText(txt);
            this->runSimulation();
        };
    }

    // Internal locagical gates (centered inside the module).
    qreal gateStartY = moduleY + padding + ((moduleHeight - (padding * 2) - ((numGates - 1) * itemSpacing)) / 2.0);
    if (numGates <= 1) gateStartY = moduleY + (moduleHeight / 2.0) - (gateHeight / 2.0);

    for (size_t i = 0; i < numGates; ++i) {
        const auto& gate = currentCircuit.gates.at(i);
        qreal currY = gateStartY + (i * itemSpacing);

        auto *gateRect = scene->addRect(moduleX + 150, currY, gateWidth, gateHeight, QPen(Qt::darkBlue, 2), QBrush(QColor(240, 248, 255)));
        
        gateRect->setData(0, QString::fromStdString(gate.name));
        gateRect->setData(1, QString::fromStdString(gate.type));
        gateRect->setData(2, QString::fromStdString(gate.output));
        gateRect->setData(3, "GATE");

        auto *gateText = scene->addText(QString::fromStdString(gate.type + ": " + gate.name + "\nIn: [Z, Z]\nOut: Z"));
        gateText->setFont(QFont("Courier New", 9));
        gateText->setPos(moduleX + 155, currY + 5);
        gateText->setParentItem(gateRect);

        // Poziția pinului de ieșire al porții pornește din capătul drept al dreptunghiului ei
        pinPositions[gate.output] = QPointF(moduleX + 150 + gateWidth, currY + (gateHeight / 2.0));
    }

    // Outputs (centered on the right side).
    qreal outStartY = moduleY + padding + ((moduleHeight - (padding * 2) - ((numOutputs - 1) * itemSpacing)) / 2.0);
    if (numOutputs <= 1) outStartY = moduleY + (moduleHeight / 2.0) - 15;

    for (size_t i = 0; i < numOutputs; ++i) {
        std::string output = currentCircuit.outputs.at(i);
        qreal currY = outStartY + (i * itemSpacing);

        auto *outItem = new VisualOutputItem(output, moduleX + moduleWidth, currY);
        outItem->setData(0, QString::fromStdString(output));
        outItem->setData(3, "OUTPUT");
        scene->addItem(outItem);

        auto *outText = scene->addText(QString::fromStdString(output + " (Z)"));
        outText->setPos(moduleX + moduleWidth + 5, currY + 2);

        pinPositions["OUT_" + output] = QPointF(moduleX + moduleWidth, currY + 15);
    }

    // 6. Wire dynamic rendering (adaptive routing).
    for (size_t i = 0; i < numGates; ++i) {
        const auto& gate = currentCircuit.gates.at(i);
        qreal currY = gateStartY + (i * itemSpacing);

        int idx = 0;
        for (const auto& inputWire : gate.inputs) {
            if (pinPositions.count(inputWire)) {
                QPointF startPoint = pinPositions[inputWire];
                // Input pins routing based on current position.
                QPointF endPoint(moduleX + 150, currY + (idx == 0 ? 18 : 48));

                auto *lineItem = scene->addLine(QLineF(startPoint, endPoint), QPen(QColor(255, 165, 0), 2));
                lineItem->setData(0, QString::fromStdString(inputWire));
            }
            idx++;
        }

        if (pinPositions.count("OUT_" + gate.output)) {
            QPointF startPoint = pinPositions[gate.output];
            QPointF endPoint = pinPositions["OUT_" + gate.output];

            auto *lineItem = scene->addLine(QLineF(startPoint, endPoint), QPen(QColor(255, 165, 0), 2));
            lineItem->setData(0, QString::fromStdString(gate.output));
        }
    }

    this->runSimulation();
}

void MainWindow::runSimulation() {
    std::map<std::string, int> wireValues = inputStates;

    // Backend boolean computation.
    for (const auto& gate : currentCircuit.gates) {
        int in1 = -1;
        int in2 = -1;

        if (gate.inputs.size() > 0) {
            std::string pinName = gate.inputs.at(0);
            if (wireValues.count(pinName) > 0) in1 = wireValues[pinName];
        }
        if (gate.inputs.size() > 1) {
            std::string pinName = gate.inputs.at(1);
            if (wireValues.count(pinName) > 0) in2 = wireValues[pinName];
        }
        
        int outValue = -1;
        if (in1 == -1 || in2 == -1) {
            if (gate.type == "and" && (in1 == 0 || in2 == 0)) outValue = 0;
            else outValue = -1;
        } else {
            if (gate.type == "and")  outValue = in1 & in2;
            else if (gate.type == "xor") outValue = in1 ^ in2;
            else if (gate.type == "or")  outValue = in1 | in2;
        }
        wireValues[gate.output] = outValue;
    }

    // Update the graphical objects.
    QList<QGraphicsItem*> itemsList = scene->items();
    for (auto* item : itemsList) {
        
        // Wire color update.
        if (item->type() == QGraphicsLineItem::Type && !item->data(0).toString().isEmpty()) {
            std::string wireName = item->data(0).toString().toStdString();
            int val = (wireValues.count(wireName) > 0) ? wireValues[wireName] : -1;

            auto* line = static_cast<QGraphicsLineItem*>(item);
            if (val == 1)      line->setPen(QPen(QColor(255, 90, 90), 3));   // Roșu
            else if (val == 0) line->setPen(QPen(QColor(65, 105, 225), 3));  // Albastru
            else               line->setPen(QPen(QColor(255, 165, 0), 2));   // Portocaliu (Z)
        }

        // Update the internal logical gates.
        if (item->type() == QGraphicsRectItem::Type && item->data(3).toString() == "GATE") {
            QString gName = item->data(0).toString();
            QString gType = item->data(1).toString();
            QString gOutput = item->data(2).toString();

            int p1 = -1;
            int p2 = -1;

            for (const auto& gate : currentCircuit.gates) {
                if (gate.name == gName.toStdString()) {
                    if (gate.inputs.size() > 0) {
                        std::string pinName = gate.inputs.at(0);
                        if (wireValues.count(pinName) > 0) p1 = wireValues[pinName];
                    }
                    if (gate.inputs.size() > 1) {
                        std::string pinName = gate.inputs.at(1);
                        if (wireValues.count(pinName) > 0) p2 = wireValues[pinName];
                    }
                    break;
                }
            }

            int finalOut = (wireValues.count(gOutput.toStdString()) > 0) ? wireValues[gOutput.toStdString()] : -1;

            auto* rect = static_cast<QGraphicsRectItem*>(item);
            if (finalOut == 1)      rect->setBrush(QBrush(QColor(152, 251, 152))); // (1) Green
            else if (finalOut == 0) rect->setBrush(QBrush(QColor(230, 230, 230))); // (0) Gray
            else                    rect->setBrush(QBrush(QColor(255, 235, 205))); // (Z) Orange

            auto children = rect->childItems();
            if (!children.isEmpty()) {
                auto* textItem = qgraphicsitem_cast<QGraphicsTextItem*>(children.first());
                if (textItem) {
                    auto formatPin = [](int v) { return (v == -1) ? "Z" : std::to_string(v); };
                    QString statusText = gType.toUpper() + ": " + gName + "\n" +
                                         "In:  [" + QString::fromStdString(formatPin(p1)) + ", " + QString::fromStdString(formatPin(p2)) + "]\n" +
                                         "Out: [" + (finalOut == -1 ? "Z" : QString::number(finalOut)) + "]";
                    textItem->setPlainText(statusText);
                }
            }
        }

        // Update ouput blocks.
        if (item->type() == QGraphicsRectItem::Type && item->data(3).toString() == "OUTPUT") {
            std::string outName = item->data(0).toString().toStdString();
            int finalOut = (wireValues.count(outName) > 0) ? wireValues[outName] : -1;

            auto* rect = static_cast<QGraphicsRectItem*>(item);
            if (finalOut == 1)      rect->setBrush(QBrush(QColor(144, 238, 144))); // (1) Green
            else if (finalOut == 0) rect->setBrush(QBrush(QColor(220, 220, 220))); // (0) Gray
            else                    rect->setBrush(QBrush(QColor(255, 222, 173))); // (Z) Orange

            for (auto* sibling : scene->items()) {
                auto* textItem = qgraphicsitem_cast<QGraphicsTextItem*>(sibling);
                if (textItem && textItem->parentItem() == nullptr && 
                    textItem->toPlainText().startsWith(QString::fromStdString(outName))) {
                    QString status = QString::fromStdString(outName) + 
                                     (finalOut == 1 ? " (1)" : (finalOut == 0 ? " (0)" : " (Z)"));
                    textItem->setPlainText(status);
                }
            }
        }
    }
}