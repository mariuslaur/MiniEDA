#include "mainWindow.hpp"
#include "hoverSplitter.hpp"
#include "parser.hpp"
#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QShortcut>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QVBoxLayout>
#include <fstream>
#include <sstream>

// Qt 6.8+ switches the colour scheme natively (and follows the system since 6.5);
// older versions get a Fusion style with a hand-made dark palette instead.
#define MINIEDA_NATIVE_COLOR_SCHEME (QT_VERSION >= QT_VERSION_CHECK(6, 8, 0))
#define MINIEDA_SYSTEM_COLOR_SCHEME (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))

namespace {

const int kSplitterHandleWidth = 6; // Wide enough to grab easily

#if !MINIEDA_NATIVE_COLOR_SCHEME
QPalette darkPalette() {
    QPalette p;
    const QColor window(45, 45, 45), base(30, 30, 30), text(230, 230, 230);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::ToolTipBase, window);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, QColor(150, 150, 150));
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor(53, 53, 53));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, QColor(80, 160, 240));
    p.setColor(QPalette::Highlight, QColor(42, 130, 218));
    p.setColor(QPalette::HighlightedText, Qt::black);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        p.setColor(QPalette::Disabled, role, QColor(127, 127, 127));
    }
    return p;
}
#endif

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    resize(1100, 650);

    m_editor = new QTextEdit(this);
    m_editor->setPlaceholderText("// Write here or open file with verilog code...");
    auto* btnOpen = new QPushButton("Open File", this);
    auto* btnSimulate = new QPushButton("Simulate", this);

    auto* leftPanel = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(m_editor);
    leftLayout->addWidget(btnOpen);
    leftLayout->addWidget(btnSimulate);

    m_scene = new MiniEDA::SchematicScene(this);
    m_view = new ZoomableGraphicsView(m_scene, this);

    auto* rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addLayout(createZoomBar());
    rightLayout->addWidget(m_view);

    // Draggable divider between the editor and the schematic, highlighted on mouse over.
    // Neither panel can be collapsed; a fitted schematic re-fits as its panel resizes.
    auto* splitter = new HoverSplitter(Qt::Horizontal, this);
    splitter->setObjectName("mainSplitter");
    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(kSplitterHandleWidth);
    splitter->setStretchFactor(0, 1); // Extra width on window resize goes 1:2
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({1000, 2000}); // Initial ratio 1:2, scaled to the available width

    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->addWidget(splitter);

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

    // Theme toggle on the right; the schematic itself keeps its light boxes and black text
    m_themeButton = new QPushButton(this);
    m_themeButton->setObjectName("themeButton");
    m_themeButton->setToolTip("Switch between dark and light theme");
#if MINIEDA_SYSTEM_COLOR_SCHEME
    m_darkTheme = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme scheme) {
        m_darkTheme = scheme == Qt::ColorScheme::Dark; // Theme changed by the system
        updateThemeButton();
    });
#endif
    updateThemeButton();
    connect(m_themeButton, &QPushButton::clicked, this, &MainWindow::toggleTheme);

    auto* zoomBar = new QHBoxLayout();
    zoomBar->setObjectName("zoomBar");
    zoomBar->addWidget(btnFit);
    zoomBar->addWidget(btnActual);
    zoomBar->addWidget(btnZoomOut);
    zoomBar->addWidget(btnZoomIn);
    zoomBar->addWidget(zoomLabel);
    zoomBar->addStretch();
    zoomBar->addWidget(m_themeButton);

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

void MainWindow::toggleTheme() {
    applyTheme(!m_darkTheme);
}

void MainWindow::applyTheme(bool dark) {
    m_darkTheme = dark;
#if MINIEDA_NATIVE_COLOR_SCHEME
    QGuiApplication::styleHints()->setColorScheme(dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
#else
    // Fusion renders any palette faithfully, so both themes look consistent
    if (QApplication::style()->name().compare("fusion", Qt::CaseInsensitive) != 0) {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
    }
    QApplication::setPalette(dark ? darkPalette() : QApplication::style()->standardPalette());
#endif
    updateThemeButton();
}

void MainWindow::updateThemeButton() {
    // The button names the theme a click switches to
    m_themeButton->setText(m_darkTheme ? "Light Theme" : "Dark Theme");
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

bool MainWindow::loadDesign(const std::string& verilogCode) {
    try {
        MiniEDA::Netlist netlist = MiniEDA::Parser().parse(verilogCode);
        if (netlist.gates.empty()) {
            QMessageBox::warning(this, "Simulation Notice", "The syntax is valid, but no logic gates were discovered to simulate.");
            return false;
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
        return true;
    } catch (const MiniEDA::SyntaxException& ex) {
        QString errorMsg = QString("Syntax Error: %1\nLine: %2, Column: %3")
                               .arg(QString::fromStdString(ex.rawMessage()))
                               .arg(ex.line())
                               .arg(ex.column());
        QMessageBox::critical(this, "Parser Failure", errorMsg);
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Fatal Error", QString::fromStdString(ex.what()));
    }
    return false;
}

void MainWindow::onInputToggled(const std::string& net, MiniEDA::LogicState state) {
    if (!m_simulator) return;
    m_simulator->setInput(net, state);
    m_simulator->evaluate();
    m_scene->refresh(*m_simulator);
}
