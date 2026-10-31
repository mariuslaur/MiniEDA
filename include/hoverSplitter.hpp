#pragma once
#include <QSplitter>
#include <QSplitterHandle>

// Splitter whose divider lights up while the mouse is over it.
// The highlight is painted directly instead of using a style sheet: a style sheet would also
// apply to every child widget and stop them from following theme (palette) changes.
class HoverSplitterHandle : public QSplitterHandle {
public:
    HoverSplitterHandle(Qt::Orientation orientation, QSplitter* parent) : QSplitterHandle(orientation, parent) {}

    bool isHighlighted() const { return m_hovered; }

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_hovered = false;
};

class HoverSplitter : public QSplitter {
public:
    using QSplitter::QSplitter;

protected:
    QSplitterHandle* createHandle() override { return new HoverSplitterHandle(orientation(), this); }
};
