#include <QtTest>
#include <QDir>
#include "schematicLayout.hpp"
#include "testUtils.hpp"

using MiniEDA::LayoutMetrics;
using MiniEDA::SchematicLayout;

class LayoutTest : public QObject {
    Q_OBJECT

private slots:
    void testExamplesHaveNoOverlappingWires();
    void testPlacementMatchesNetlist();
    void testEveryRoutablePinIsWired();
};

namespace {

struct Segment {
    std::string net;
    QPointF a, b;
    bool horizontal() const { return qFuzzyCompare(a.y(), b.y()); }
    bool vertical() const { return qFuzzyCompare(a.x(), b.x()); }
};

std::vector<Segment> segmentsOf(const SchematicLayout& layout) {
    std::vector<Segment> segments;
    for (const auto& wire : layout.wires) {
        for (const auto& branch : wire.branches) {
            for (size_t i = 1; i < branch.size(); ++i) {
                if (branch[i - 1] != branch[i]) segments.push_back({wire.net, branch[i - 1], branch[i]});
            }
        }
    }
    return segments;
}

// Length shared by two collinear axis-aligned segments (0 when they only cross or touch)
qreal collinearOverlap(const Segment& s, const Segment& t) {
    auto overlap = [](qreal a1, qreal a2, qreal b1, qreal b2) {
        return std::min(std::max(a1, a2), std::max(b1, b2)) - std::max(std::min(a1, a2), std::min(b1, b2));
    };
    if (s.horizontal() && t.horizontal() && std::abs(s.a.y() - t.a.y()) < 3.0)
        return overlap(s.a.x(), s.b.x(), t.a.x(), t.b.x());
    if (s.vertical() && t.vertical() && std::abs(s.a.x() - t.a.x()) < 3.0)
        return overlap(s.a.y(), s.b.y(), t.a.y(), t.b.y());
    return 0.0;
}

// True when an axis-aligned segment passes through the inside of a box (touching its border is fine)
bool crossesInterior(const Segment& s, const QRectF& box) {
    QRectF inner = box.adjusted(0.5, 0.5, -0.5, -0.5);
    QRectF span = QRectF(s.a, s.b).normalized();
    if (s.horizontal()) span.adjust(0, -0.01, 0, 0.01);
    if (s.vertical()) span.adjust(-0.01, 0, 0.01, 0);
    return inner.intersects(span);
}

// Two metric sets: the defaults and a large-gate variant (long labels)
std::vector<LayoutMetrics> metricVariants() {
    LayoutMetrics wide;
    wide.inputBoxWidth = 140;
    wide.outputBoxWidth = 120;
    wide.gateWidth = 320;
    wide.gateHeight = 90;
    return {LayoutMetrics{}, wide};
}

QStringList exampleFiles() {
    return QDir(MINIEDA_VERILOG_DIR).entryList({"*.v"}, QDir::Files, QDir::Name);
}

} // namespace

void LayoutTest::testExamplesHaveNoOverlappingWires() {
    const QStringList files = exampleFiles();
    QVERIFY(!files.isEmpty());
    for (const QString& file : files) {
        MiniEDA::Netlist netlist = TestUtils::parse(TestUtils::readExample(file.toStdString()));
        for (const LayoutMetrics& metrics : metricVariants()) {
            SchematicLayout layout = MiniEDA::computeSchematicLayout(netlist, metrics);
            std::vector<Segment> segments = segmentsOf(layout);

            for (size_t i = 0; i < segments.size(); ++i) {
                const Segment& s = segments[i];
                QVERIFY2(s.horizontal() || s.vertical(), qPrintable(file + ": diagonal segment on " + QString::fromStdString(s.net)));

                // Wires of different nets may cross but never run on top of each other
                for (size_t j = i + 1; j < segments.size(); ++j) {
                    const Segment& t = segments[j];
                    if (s.net == t.net) continue;
                    QVERIFY2(collinearOverlap(s, t) <= 1.0,
                             qPrintable(file + ": " + QString::fromStdString(s.net) + " overlaps " + QString::fromStdString(t.net)));
                }

                // No wire runs through a gate body
                for (size_t g = 0; g < layout.gates.size(); ++g) {
                    QVERIFY2(!crossesInterior(s, layout.gates[g]),
                             qPrintable(file + ": " + QString::fromStdString(s.net) + " crosses gate " +
                                        QString::fromStdString(netlist.gates[g].name)));
                }

                // Wires stay inside the module frame
                QRectF frame = layout.moduleFrame.adjusted(-1, -1, 1, 1);
                QVERIFY2(frame.contains(s.a) && frame.contains(s.b),
                         qPrintable(file + ": " + QString::fromStdString(s.net) + " leaves the module frame"));
            }
        }
    }
}

void LayoutTest::testPlacementMatchesNetlist() {
    MiniEDA::Netlist netlist = TestUtils::parse(TestUtils::readExample("07_mux_4to1.v"));
    LayoutMetrics metrics;
    SchematicLayout layout = MiniEDA::computeSchematicLayout(netlist, metrics);

    QCOMPARE(layout.inputs.size(), netlist.inputs.size());
    QCOMPARE(layout.outputs.size(), netlist.outputs.size());
    QCOMPARE(layout.gates.size(), netlist.gates.size());

    for (size_t i = 0; i < layout.inputs.size(); ++i) {
        const auto& port = layout.inputs[i];
        QCOMPARE(port.net, netlist.inputs[i]);
        QCOMPARE(port.box.width(), metrics.inputBoxWidth);
        QCOMPARE(port.box.right(), layout.moduleFrame.left()); // Inputs sit on the left frame edge
        QCOMPARE(port.pin.x(), layout.moduleFrame.left());
    }
    for (const auto& port : layout.outputs) {
        QCOMPARE(port.box.width(), metrics.outputBoxWidth);
        QCOMPARE(port.box.left(), layout.moduleFrame.right()); // Outputs sit on the right frame edge
    }
    for (size_t g = 0; g < layout.gates.size(); ++g) {
        QCOMPARE(layout.gates[g].size(), QSizeF(metrics.gateWidth, metrics.gateHeight));
        QVERIFY(layout.moduleFrame.contains(layout.gates[g]));
        if (g > 0) QVERIFY(layout.gates[g].top() > layout.gates[g - 1].bottom()); // Gates don't overlap
    }
}

void LayoutTest::testEveryRoutablePinIsWired() {
    // Every gate input fed by a primary input or another gate must have a branch ending on that pin
    MiniEDA::Netlist netlist = TestUtils::parse(TestUtils::readExample("08_univ_shift.v"));
    SchematicLayout layout = MiniEDA::computeSchematicLayout(netlist, LayoutMetrics{});

    std::map<std::string, std::vector<QPointF>> endpoints;
    for (const auto& wire : layout.wires) {
        for (const auto& branch : wire.branches) endpoints[wire.net].push_back(branch.back());
    }

    size_t wiredPins = 0;
    for (size_t g = 0; g < netlist.gates.size(); ++g) {
        const auto& gate = netlist.gates[g];
        for (const auto& net : gate.inputWires) {
            QVERIFY2(endpoints.count(net), qPrintable(QString::fromStdString(gate.name + " input " + net + " is not wired")));
            bool endsOnGate = std::any_of(endpoints[net].begin(), endpoints[net].end(), [&](const QPointF& p) {
                return qFuzzyCompare(p.x(), layout.gates[g].left()) &&
                       p.y() > layout.gates[g].top() && p.y() < layout.gates[g].bottom();
            });
            QVERIFY2(endsOnGate, qPrintable(QString::fromStdString(gate.name + " input " + net + " has no wire ending on it")));
            ++wiredPins;
        }
    }
    QVERIFY(wiredPins > 0);

    // Each output driven by a gate is wired to its port
    for (const auto& port : layout.outputs) {
        const auto& ends = endpoints[port.net];
        QVERIFY2(std::find(ends.begin(), ends.end(), port.pin) != ends.end(),
                 qPrintable(QString::fromStdString(port.net + " is not wired to its output port")));
    }
}

QTEST_MAIN(LayoutTest)
#include "layoutTest.moc"
