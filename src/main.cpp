#include <QApplication>
#include "mainWindow.hpp"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    MainWindow window;
    window.setWindowTitle("MiniEDA - Verilog Netlist Visual Simulator");
    window.show();
    
    return app.exec();
}