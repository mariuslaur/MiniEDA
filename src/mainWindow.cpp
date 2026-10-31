#include "mainWindow.hpp"
#include "parser.hpp"
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>
#include <fstream>
#include <sstream>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    resize(1100, 650);

    m_editor = new QTextEdit(this);
    m_editor->setPlaceholderText("// Write here or open file with verilog code...");
    auto* btnOpen = new QPushButton("Open File", this);
    auto* btnSimulate = new QPushButton("Simulate", this);

    auto* leftLayout = new QVBoxLayout();
    leftLayout->addWidget(m_editor);
    leftLayout->addWidget(btnOpen);
    leftLayout->addWidget(btnSimulate);

    m_scene = new MiniEDA::SchematicScene(this);
    m_view = new ZoomableGraphicsView(m_scene, this);

    auto* rightLayout = new QVBoxLayout();
    rightLayout->addLayout(createZoomBar());
    rightLayout->addWidget(m_view);

    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addLayout(rightLayout, 2);

    connect(btnOpen, &QPushButton::clicked, this, &MainWindow::openFile);
    connect(btnSimulate, &QPushButton::clicked, this, [this]() {
        loadDesign(m_editor->toPlainText().toStdString());
    });
    connect(m_scene, &MiniEDA::SchematicScene::inputToggled, this, &MainWindow::onInputToggled);
}

QLayout* MainWindow::createZoomBar() {
    auto* btnFit = new QPushButton("Fit", this);
    auto* btnActual = new QPushButton("100%", this);
    auto* btnZoomOut = new QPushButton("-", this);
    auto* btnZoomIn = new QPushButton("+", this);
    auto* zoomLabel = new QLabel("100%", this);
    btnFit->setToolTip("Fit circuit to window (Ctrl+0)");
    btnActual->setToolTip("Actual size (Ctrl+1)");
    btnZoomOut->setToolTip("Zoom out (Ctrl+-, Ctrl+Wheel)");
    btnZoomIn->setToolTip("Zoom in (Ctrl++, Ctrl+Wheel)");
    btnZoomOut->setFixedWidth(32);
    btnZoomIn->setFixedWidth(32);
    zoomLabel->setMinimumWidth(48);

    auto* zoomBar = new QHBoxLayout();
    zoomBar->addWidget(btnFit);
    zoomBar->addWidget(btnActual);
    zoomBar->addWidget(btnZoomOut);
    zoomBar->addWidget(btnZoomIn);
    zoomBar->addWidget(zoomLabel);
    zoomBar->addStretch();

    connect(btnFit, &QPushButton::clicked, m_view, [this]() { m_view->zoomToFit(); });
    connect(btnActual, &QPushButton::clicked, m_view, &ZoomableGraphicsView::resetZoom);
    connect(btnZoomOut, &QPushButton::clicked, m_view, &ZoomableGraphicsView::zoomOut);
    connect(btnZoomIn, &QPushButton::clicked, m_view, &ZoomableGraphicsView::zoomIn);
    connect(m_view, &ZoomableGraphicsView::zoomChanged, zoomLabel, [zoomLabel](qreal level) {
        zoomLabel->setText(QString::number(qRound(level * 100)) + "%");
    });

    new QShortcut(QKeySequence("Ctrl+0"), this, [this]() { m_view->zoomToFit(); });
    new QShortcut(QKeySequence("Ctrl+1"), this, [this]() { m_view->resetZoom(); });
    new QShortcut(QKeySequence::ZoomIn, this, [this]() { m_view->zoomIn(); });
    new QShortcut(QKeySequence("Ctrl+="), this, [this]() { m_view->zoomIn(); });
    new QShortcut(QKeySequence::ZoomOut, this, [this]() { m_view->zoomOut(); });

    return zoomBar;
}

void MainWindow::openFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open Verilog Netlist", "", "Verilog Files (*.v);;Text Files (*.txt)");
    if (fileName.isEmpty()) return;

    std::ifstream file(fileName.toStdString());
    if (!file.is_open()) {
        QMessageBox::critical(this, "Open File", "Cannot open " + fileName);
        return;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    m_editor->setPlainText(QString::fromStdString(buffer.str()));
    loadDesign(buffer.str());
}

void MainWindow::loadDesign(const std::string& verilogCode) {
    try {
        MiniEDA::Netlist netlist = MiniEDA::Parser().parse(verilogCode);
        if (netlist.gates.empty()) {
            QMessageBox::warning(this, "Simulation Notice", "The syntax is valid, but no logic gates were discovered to simulate.");
            return;
        }

        auto simulator = std::make_unique<MiniEDA::Simulator>(std::move(netlist));
        // Inputs start at a defined 0, like real signals: a CLK or RST left on Z would block flip-flops
        for (const auto& input : simulator->netlist().inputs) {
            simulator->setInput(input, MiniEDA::LogicState::LogicLow);
        }
        simulator->evaluate();

        m_simulator = std::move(simulator);
        m_scene->build(*m_simulator, m_view->viewport()->size());
        // Large circuits are shrunk to fit the window; small ones stay at 100% for readability
        m_view->zoomToFit(false);
    } catch (const MiniEDA::SyntaxException& ex) {
        QString errorMsg = QString("Syntax Error: %1\nLine: %2, Column: %3")
                               .arg(QString::fromStdString(ex.rawMessage()))
                               .arg(ex.line())
                               .arg(ex.column());
        QMessageBox::critical(this, "Parser Failure", errorMsg);
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Fatal Error", QString::fromStdString(ex.what()));
    }
}

void MainWindow::onInputToggled(const std::string& net, MiniEDA::LogicState state) {
    if (!m_simulator) return;
    m_simulator->setInput(net, state);
    m_simulator->evaluate();
    m_scene->refresh(*m_simulator);
}
