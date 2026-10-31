#pragma once
#include <QGraphicsView>
#include <QResizeEvent>
#include <QWheelEvent>

// Graphics view with Ctrl + mouse wheel zoom, fit-to-window and drag-to-pan
class ZoomableGraphicsView : public QGraphicsView {
    Q_OBJECT
public:
    static constexpr qreal kZoomStep = 1.2; // One wheel notch / one button press

    explicit ZoomableGraphicsView(QGraphicsScene* scene, QWidget* parent = nullptr);

    void zoomBy(qreal factor);                // Relative zoom anchored at the viewport center
    void zoomIn() { zoomBy(kZoomStep); }
    void zoomOut() { zoomBy(1.0 / kZoomStep); }
    void zoomToFit(bool allowEnlarge = true); // Fit the whole circuit; stays fitted while the window resizes
    void resetZoom();                         // 100%
    qreal zoomLevel() const { return transform().m11(); }

signals:
    void zoomChanged(qreal level);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    bool m_fitMode = false;
    bool m_fitAllowEnlarge = true;
    void applyZoom(qreal level);
};
