#pragma once
#include "schematicScene.hpp"
#include "simulator.hpp"
#include "zoomableView.hpp"
#include <QMainWindow>
#include <QTextEdit>
#include <memory>

// Editor on the left, zoomable schematic on the right. Owns the simulator of the loaded design.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QTextEdit* m_editor;
    MiniEDA::SchematicScene* m_scene;
    ZoomableGraphicsView* m_view;
    std::unique_ptr<MiniEDA::Simulator> m_simulator;

    QLayout* createZoomBar();
    void openFile();
    // Parses, simulates and draws a design; reports syntax and runtime errors in a dialog
    void loadDesign(const std::string& verilogCode);
    void onInputToggled(const std::string& net, MiniEDA::LogicState state);
};
