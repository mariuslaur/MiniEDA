#include <QtTest>
#include <QGraphicsTextItem>
#include <QHBoxLayout>
#include <QSplitter>
#include "hoverSplitter.hpp"
#include "mainWindow.hpp"
#include "testUtils.hpp"

using MiniEDA::InputPortItem;
using MiniEDA::LogicState;
using MiniEDA::OutputPortItem;

class UiTest : public QObject {
    Q_OBJECT

private slots:
    void testLoadDrawsEveryElement();
    void testClickingInputsDrivesTheShiftRegister();
    void testSchematicTextIsBlackWithDarkPalette();
    void testThemeButtonToggles();
    void testThemeButtonSitsRightOfZoomControls();
    void testSplitterSeparatesEditorAndSchematic();
    void testDraggingTheSplitterResizesBothPanels();
    void testPanelsAndButtonsFollowTheTheme();
    void testNoStyleSheetsBlockThemeChanges();
};

namespace {

ZoomableGraphicsView* viewOf(MainWindow& window) {
    return window.findChild<ZoomableGraphicsView*>();
}

template <typename Item>
std::vector<Item*> itemsOf(MainWindow& window) {
    std::vector<Item*> result;
    for (auto* item : viewOf(window)->scene()->items()) {
        if (auto* typed = dynamic_cast<Item*>(item)) result.push_back(typed);
    }
    return result;
}

template <typename Item>
Item* portNamed(MainWindow& window, const std::string& net) {
    for (auto* item : itemsOf<Item>(window)) {
        if (item->net() == net) return item;
    }
    return nullptr;
}

// Press, move with the left button held, and release on the splitter handle
void dragSplitterHandle(QSplitter* splitter, int dx) {
    QSplitterHandle* handle = splitter->handle(1);
    const QPointF start = QRectF(handle->rect()).center();
    const QPointF end = start + QPointF(dx, 0);
    const struct {
        QEvent::Type type;
        QPointF local;
        Qt::MouseButtons buttons;
    } steps[] = {
        {QEvent::MouseButtonPress, start, Qt::LeftButton},
        {QEvent::MouseMove, end, Qt::LeftButton},
        {QEvent::MouseButtonRelease, end, Qt::NoButton},
    };
    for (const auto& step : steps) {
        QMouseEvent event(step.type, step.local, handle->mapToGlobal(step.local), Qt::LeftButton, step.buttons,
                          Qt::NoModifier);
        QApplication::sendEvent(handle, &event);
    }
}

void clickInput(MainWindow& window, const std::string& net) {
    auto* view = viewOf(window);
    auto* port = portNamed<InputPortItem>(window, net);
    QVERIFY2(port, net.c_str());
    view->centerOn(port);
    QTest::mouseClick(view->viewport(), Qt::LeftButton, {}, view->mapFromScene(port->sceneBoundingRect().center()));
}

} // namespace

void UiTest::testLoadDrawsEveryElement() {
    MainWindow window;
    QVERIFY(window.loadDesign(TestUtils::readExample("07_mux_4to1.v")));
    MiniEDA::Netlist netlist = TestUtils::parse(TestUtils::readExample("07_mux_4to1.v"));

    QCOMPARE(itemsOf<MiniEDA::GateItem>(window).size(), netlist.gates.size());
    QCOMPARE(itemsOf<InputPortItem>(window).size(), netlist.inputs.size());
    QCOMPARE(itemsOf<OutputPortItem>(window).size(), netlist.outputs.size());
    for (auto* input : itemsOf<InputPortItem>(window)) {
        QCOMPARE(input->state(), LogicState::LogicLow); // Inputs start at 0
    }
}

void UiTest::testClickingInputsDrivesTheShiftRegister() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.loadDesign(TestUtils::readExample("08_univ_shift.v")));
    viewOf(window)->resetZoom();

    // Reset pulse, parallel load mode, data 11, then a rising clock edge
    for (const char* net : {"RST", "RST", "S1", "S0", "I0", "I1", "CLK"}) clickInput(window, net);

    QCOMPARE(portNamed<InputPortItem>(window, "CLK")->state(), LogicState::LogicHigh);
    for (const char* net : {"Q0", "Q1"}) {
        QCOMPARE(portNamed<OutputPortItem>(window, net)->brush().color(),
                 MiniEDA::Palette::outputFill(LogicState::LogicHigh));
    }
}

void UiTest::testSchematicTextIsBlackWithDarkPalette() {
    // Simulate a dark system theme: the palette's text colour becomes white
    const QPalette original = QApplication::palette();
    QPalette dark = original;
    dark.setColor(QPalette::Text, Qt::white);
    dark.setColor(QPalette::WindowText, Qt::white);
    QApplication::setPalette(dark);

    MainWindow window;
    QVERIFY(window.loadDesign(TestUtils::readExample("04_mixed_ctrl_unit.v")));
    int texts = 0;
    for (auto* item : viewOf(window)->scene()->items()) {
        if (auto* text = dynamic_cast<QGraphicsTextItem*>(item)) {
            QCOMPARE(text->defaultTextColor(), QColor(Qt::black));
            ++texts;
        }
    }
    QVERIFY(texts > 0);
    QApplication::setPalette(original);
}

void UiTest::testThemeButtonToggles() {
    MainWindow window;
    auto* button = window.findChild<QPushButton*>("themeButton");
    QVERIFY(button);

    const bool startDark = window.isDarkTheme();
    // The button names the theme a click switches to
    QCOMPARE(button->text(), startDark ? QString("Light Theme") : QString("Dark Theme"));

    QTest::mouseClick(button, Qt::LeftButton);
    QCOMPARE(window.isDarkTheme(), !startDark);
    QCOMPARE(button->text(), !startDark ? QString("Light Theme") : QString("Dark Theme"));
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
    // Fallback theme: the application palette really changes
    QCOMPARE(QApplication::palette().color(QPalette::Window).lightness() < 128, !startDark);
#endif

    QTest::mouseClick(button, Qt::LeftButton);
    QCOMPARE(window.isDarkTheme(), startDark);
    QCOMPARE(button->text(), startDark ? QString("Light Theme") : QString("Dark Theme"));
}

void UiTest::testThemeButtonSitsRightOfZoomControls() {
    MainWindow window;
    auto* bar = window.findChild<QHBoxLayout*>("zoomBar");
    QVERIFY(bar);
    QCOMPARE(qobject_cast<QPushButton*>(bar->itemAt(0)->widget())->text(), QString("Fit"));
    QCOMPARE(bar->itemAt(bar->count() - 1)->widget(), window.findChild<QPushButton*>("themeButton"));
    QVERIFY(bar->itemAt(bar->count() - 2)->spacerItem()); // Pushed right by the stretch
}

void UiTest::testSplitterSeparatesEditorAndSchematic() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* splitter = window.findChild<QSplitter*>("mainSplitter");
    QVERIFY(splitter);

    QCOMPARE(splitter->count(), 2);
    QVERIFY(splitter->widget(0)->isAncestorOf(window.findChild<QTextEdit*>()));
    QVERIFY(splitter->widget(1)->isAncestorOf(viewOf(window)));
    QVERIFY(!splitter->childrenCollapsible());

    const QList<int> sizes = splitter->sizes();
    QVERIFY(qAbs(sizes[1] - 2 * sizes[0]) <= sizes[1] / 20); // Initial ratio 1:2
    auto* handle = dynamic_cast<HoverSplitterHandle*>(splitter->handle(1));
    QVERIFY(handle);
    QCOMPARE(handle->cursor().shape(), Qt::SplitHCursor); // Resize cursor on mouse over

    // Highlighted while the mouse is over the handle
    const QPointF center = QRectF(handle->rect()).center();
    QEnterEvent enter(center, center, handle->mapToGlobal(center));
    QApplication::sendEvent(handle, &enter);
    QVERIFY(handle->isHighlighted());
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(handle, &leave);
    QVERIFY(!handle->isHighlighted());
}

void UiTest::testPanelsAndButtonsFollowTheTheme() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* editor = window.findChild<QTextEdit*>();
    auto* themeButton = window.findChild<QPushButton*>("themeButton");

    auto checkFollowsApplicationPalette = [&]() {
        const QPalette app = QApplication::palette();
        QCOMPARE(editor->palette().color(QPalette::Base), app.color(QPalette::Base));
        QCOMPARE(viewOf(window)->palette().color(QPalette::Base), app.color(QPalette::Base));
        QCOMPARE(themeButton->palette().color(QPalette::Button), app.color(QPalette::Button));
        QCOMPARE(themeButton->palette().color(QPalette::ButtonText), app.color(QPalette::ButtonText));
    };

    const bool startDark = window.isDarkTheme();
    for (int click = 1; click <= 2; ++click) {
        QTest::mouseClick(themeButton, Qt::LeftButton);
        QCoreApplication::processEvents();
        checkFollowsApplicationPalette();
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
        // Fallback theme: the panel background really is dark / light
        const bool dark = (click == 1) != startDark;
        QCOMPARE(editor->palette().color(QPalette::Base).lightness() < 128, dark);
#endif
    }
}

void UiTest::testNoStyleSheetsBlockThemeChanges() {
    // Style sheets freeze the colours of a widget and its children, so the theme switch would skip them
    MainWindow window;
    QVERIFY(window.styleSheet().isEmpty());
    for (auto* widget : window.findChildren<QWidget*>()) {
        QVERIFY2(widget->styleSheet().isEmpty(), qPrintable(widget->metaObject()->className()));
    }
}

void UiTest::testDraggingTheSplitterResizesBothPanels() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* splitter = window.findChild<QSplitter*>("mainSplitter");
    const QList<int> sizes = splitter->sizes();
    const int viewWidth = viewOf(window)->width();

    dragSplitterHandle(splitter, 150);
    const QList<int> moved = splitter->sizes();
    QVERIFY(qAbs(moved[0] - (sizes[0] + 150)) <= 2);
    QCOMPARE(moved[0] + moved[1], sizes[0] + sizes[1]);
    QVERIFY(viewOf(window)->width() < viewWidth); // The schematic panel really shrank

    dragSplitterHandle(splitter, -100000); // Far beyond the window: the editor keeps its minimum width
    QVERIFY(splitter->sizes()[0] > 0 && splitter->sizes()[1] > 0);
}

QTEST_MAIN(UiTest)
#include "uiTest.moc"
