#include "hoverSplitter.hpp"
#include <QPainter>

void HoverSplitterHandle::enterEvent(QEnterEvent* event) {
    m_hovered = true;
    update();
    QSplitterHandle::enterEvent(event);
}

void HoverSplitterHandle::leaveEvent(QEvent* event) {
    m_hovered = false;
    update();
    QSplitterHandle::leaveEvent(event);
}

void HoverSplitterHandle::paintEvent(QPaintEvent* event) {
    if (m_hovered) {
        // The theme's accent colour, so the highlight fits both dark and light mode
        QPainter(this).fillRect(rect(), palette().color(QPalette::Highlight));
    } else {
        QSplitterHandle::paintEvent(event);
    }
}
