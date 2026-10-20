#pragma once
#include <QMainWindow>
#include <QTextEdit>
#include <QPushButton>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QBrush>
#include <QPen>
#include <functional>
#include "parser.hpp"

class InteractiveInputItem : public QGraphicsRectItem {
public:
    std::string name;
    int value = -1; // Pornim din starea -1 (UNDEFINED / Z)
    std::function<void()> onClickCallback;

    InteractiveInputItem(const std::string& name, qreal x, qreal y) 
        : QGraphicsRectItem(x, y, 60, 30), name(name) {
        setBrush(QBrush(QColor(255, 165, 0))); // Portocaliu pentru starea inițială Z
        setPen(QPen(Qt::black, 2));
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            // Ciclu de stări: Undefined -> 1 -> 0 -> 1 -> 0...
            if (value == -1 || value == 0) value = 1;
            else value = 0;

            if (value == 1) setBrush(QBrush(QColor(255, 100, 100))); // Roșu
            else setBrush(QBrush(Qt::lightGray)); // Gri

            if (onClickCallback) {
                onClickCallback();
            }
        }
        QGraphicsRectItem::mousePressEvent(event);
    }
};


// Clasă pentru nodurile de Output de pe marginea dreaptă a modulului
class VisualOutputItem : public QGraphicsRectItem {
public:
    std::string name;
    int value = -1;

    VisualOutputItem(const std::string& name, qreal x, qreal y) 
        : QGraphicsRectItem(x, y, 60, 30), name(name) {
        setBrush(QBrush(QColor(255, 222, 173))); // Culoare inițială Z
        setPen(QPen(Qt::black, 2));
    }

    void updateState(int newValue) {
        value = newValue;
        if (value == 1) setBrush(QBrush(QColor(144, 238, 144))); // Verde pentru 1
        else if (value == 0) setBrush(QBrush(QColor(220, 220, 220))); // Gri pentru 0
        else setBrush(QBrush(QColor(255, 222, 173))); // Z
    }
};


class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private slots:
    void openFile();
    void runSimulation();
private:
    QTextEdit *editor;
    QGraphicsView *view;
    QGraphicsScene *scene;
    MiniEDA::Parser m_parser; 

    std::map<std::string, int> inputStates; 
    
    void renderCircuit();
};