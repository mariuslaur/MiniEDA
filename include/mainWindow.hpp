#pragma once
#include "schematicScene.hpp"
#include "simulator.hpp"
#include "zoomableView.hpp"
#include <QMainWindow>
#include <QPushButton>
#include <QTextEdit>
#include <memory>

// Editor on the left, zoomable schematic on the right. Owns the simulator of the loaded design.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Parses, simulates and draws a design; reports syntax and runtime errors in a dialog
    bool loadDesign(const std::string& verilogCode);

    // Dark/light theme; initially the system theme
    bool isDarkTheme() const { return m_darkTheme; }
    void toggleTheme();

private:
    QTextEdit* m_editor;
    MiniEDA::SchematicScene* m_scene;
    ZoomableGraphicsView* m_view;
    QPushButton* m_themeButton;
    std::unique_ptr<MiniEDA::Simulator> m_simulator;
    bool m_darkTheme = false;

    QLayout* createZoomBar();
    void openFile();
    void onInputToggled(const std::string& net, MiniEDA::LogicState state);
    void applyTheme(bool dark);
    void updateThemeButton();
};
