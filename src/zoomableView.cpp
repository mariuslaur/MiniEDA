#include "zoomableView.hpp"
#include <algorithm>
#include <cmath>

namespace {
const qreal kMinZoom = 0.05;
const qreal kMaxZoom = 8.0;
}

ZoomableGraphicsView::ZoomableGraphicsView(QGraphicsScene* scene, QWidget* parent) : QGraphicsView(scene, parent) {
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag); // Drag on empty canvas pans; clicks on inputs still toggle
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
}

void ZoomableGraphicsView::applyZoom(qreal level) {
    level = std::clamp(level, kMinZoom, kMaxZoom);
    qreal factor = level / zoomLevel();
    scale(factor, factor);
    emit zoomChanged(zoomLevel());
}

void ZoomableGraphicsView::zoomBy(qreal factor) {
    m_fitMode = false;
    applyZoom(zoomLevel() * factor);
}

void ZoomableGraphicsView::resetZoom() {
    m_fitMode = false;
    applyZoom(1.0);
}

void ZoomableGraphicsView::zoomToFit(bool allowEnlarge) {
    m_fitMode = true;
    m_fitAllowEnlarge = allowEnlarge;
    if (!scene()) return;

    QRectF bounds = scene()->sceneRect();
    QRectF viewRect = viewport()->rect();
    if (bounds.isEmpty() || viewRect.isEmpty()) return;

    qreal level = std::min(viewRect.width() / bounds.width(), viewRect.height() / bounds.height());
    if (!allowEnlarge) level = std::min(level, 1.0);
    applyZoom(level);
    centerOn(bounds.center());
}

void ZoomableGraphicsView::wheelEvent(QWheelEvent* event) {
    if (!(event->modifiers() & Qt::ControlModifier)) {
        QGraphicsView::wheelEvent(event); // Plain wheel keeps scrolling
        return;
    }
    int delta = event->angleDelta().y();
    if (delta != 0) {
        // Zoom around the cursor; 120 units = one notch of a standard mouse wheel
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        zoomBy(std::pow(kZoomStep, delta / 120.0));
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    }
    event->accept();
}

void ZoomableGraphicsView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    if (m_fitMode) zoomToFit(m_fitAllowEnlarge);
}
