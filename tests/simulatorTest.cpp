#include <QtTest>
#include "testUtils.hpp"

using MiniEDA::LogicState;
using MiniEDA::Simulator;
using TestUtils::bit;
using TestUtils::parse;

class SimulatorTest : public QObject {
    Q_OBJECT

private slots:
    void testTriStateLogicPropagation();
    void testControllingValuesResolveZ();
    void testAssignEvaluation();
    void testVectorEvaluation();
    void testMux4to1();
    void testUnknownNetThrows();
    void testUniversalShiftRegister();
};

void SimulatorTest::testTriStateLogicPropagation() {
    Simulator sim(parse(
        "module FullSet (input A, B, output Y_nand, Y_nor, Y_xnor, Y_buf, Y_not);\n"
        "  nand g1 (Y_nand, A, B);\n"
        "  nor  g2 (Y_nor, A, B);\n"
        "  xnor g3 (Y_xnor, A, B);\n"
        "  buf  g4 (Y_buf, A);\n"
        "  not  g5 (Y_not, B);\n"
        "endmodule"));

    // 1: Default (Z)
    sim.evaluate();
    QCOMPARE(sim.wireState("Y_nand"), LogicState::HighImpedance);
    QCOMPARE(sim.wireState("Y_nor"), LogicState::HighImpedance);

    // 2: A = 1, B = 0
    sim.setInput("A", LogicState::LogicHigh);
    sim.setInput("B", LogicState::LogicLow);
    sim.evaluate();

    QCOMPARE(sim.wireState("Y_nand"), LogicState::LogicHigh); // 1 NAND 0 = 1
    QCOMPARE(sim.wireState("Y_nor"), LogicState::LogicLow);   // 1 NOR 0 = 0
    QCOMPARE(sim.wireState("Y_xnor"), LogicState::LogicLow);  // 1 XNOR 0 = 0
    QCOMPARE(sim.wireState("Y_buf"), LogicState::LogicHigh);  // BUF 1 = 1
    QCOMPARE(sim.wireState("Y_not"), LogicState::LogicHigh);  // NOT 0 = 1
}

void SimulatorTest::testControllingValuesResolveZ() {
    Simulator sim(parse(
        "module m (input a, b, output y_and, y_nand, y_or, y_nor, y_xor);\n"
        "  and  g1 (y_and, a, b);\n"
        "  nand g2 (y_nand, a, b);\n"
        "  or   g3 (y_or, a, b);\n"
        "  nor  g4 (y_nor, a, b);\n"
        "  xor  g5 (y_xor, a, b);\n"
        "endmodule"));

    // b stays Z: a controlling value on a decides the output, otherwise it stays Z
    sim.setInput("a", LogicState::LogicLow);
    sim.evaluate();
    QCOMPARE(sim.wireState("y_and"), LogicState::LogicLow);
    QCOMPARE(sim.wireState("y_nand"), LogicState::LogicHigh);
    QCOMPARE(sim.wireState("y_or"), LogicState::HighImpedance);
    QCOMPARE(sim.wireState("y_nor"), LogicState::HighImpedance);
    QCOMPARE(sim.wireState("y_xor"), LogicState::HighImpedance);

    sim.setInput("a", LogicState::LogicHigh);
    sim.evaluate();
    QCOMPARE(sim.wireState("y_and"), LogicState::HighImpedance);
    QCOMPARE(sim.wireState("y_nand"), LogicState::HighImpedance);
    QCOMPARE(sim.wireState("y_or"), LogicState::LogicHigh);
    QCOMPARE(sim.wireState("y_nor"), LogicState::LogicLow);
    QCOMPARE(sim.wireState("y_xor"), LogicState::HighImpedance);
}

void SimulatorTest::testAssignEvaluation() {
    Simulator sim(parse(
        "module Dataflow (input a, b, c, d, output y, z, x, w, v);\n"
        "  wire t;\n"
        "  assign t = a;\n"
        "  assign y = ~(a & b) | c ^ d;\n"
        "  assign x = a ~^ b ~^ c;\n"
        "  assign w = a && b || !c;\n"
        "  assign v = t & b & c;\n"
        "endmodule"));

    sim.setInput("a", LogicState::LogicHigh);
    sim.setInput("b", LogicState::LogicLow);
    sim.setInput("c", LogicState::LogicLow);
    sim.setInput("d", LogicState::LogicHigh);
    sim.evaluate();

    QCOMPARE(sim.wireState("t"), LogicState::LogicHigh);
    QCOMPARE(sim.wireState("y"), LogicState::LogicHigh); // ~(1&0)=1 | (0^1)=1
    QCOMPARE(sim.wireState("x"), LogicState::LogicHigh); // (1 xnor 0)=0, 0 xnor 0 = 1
    QCOMPARE(sim.wireState("w"), LogicState::LogicHigh); // (1&&0)=0 || !0=1
    QCOMPARE(sim.wireState("v"), LogicState::LogicLow);  // 1&0&0
}

void SimulatorTest::testVectorEvaluation() {
    Simulator sim(parse(TestUtils::readExample("02_dataflow.v")));
    // assign y = (a[0] & b) | (~a[1]);  assign z = (a[0] ^ b) & y;
    sim.setInput("a[0]", LogicState::LogicHigh);
    sim.setInput("a[1]", LogicState::LogicLow);
    sim.setInput("b", LogicState::LogicHigh);
    sim.evaluate();

    QCOMPARE(sim.wireState("y"), LogicState::LogicHigh); // (1&1) | ~0
    QCOMPARE(sim.wireState("z"), LogicState::LogicLow);  // (1^1) & 1
}

void SimulatorTest::testMux4to1() {
    Simulator sim(parse(TestUtils::readExample("07_mux_4to1.v")));

    // Y must follow the selected data input for every select combination
    const char* data[] = {"D0", "D1", "D2", "D3"};
    for (int sel = 0; sel < 4; ++sel) {
        sim.setInput("S0", bit(sel & 1));
        sim.setInput("S1", bit(sel & 2));
        for (int d = 0; d < 4; ++d) sim.setInput(data[d], bit(d == sel));
        sim.evaluate();
        QCOMPARE(sim.wireState("Y"), LogicState::LogicHigh);

        sim.setInput(data[sel], LogicState::LogicLow);
        sim.evaluate();
        QCOMPARE(sim.wireState("Y"), LogicState::LogicLow);
    }
}

void SimulatorTest::testUnknownNetThrows() {
    Simulator sim(parse("module m (input a, output y);\n  not n1 (y, a);\nendmodule"));
    QVERIFY_EXCEPTION_THROWN(sim.setInput("missing", LogicState::LogicHigh), std::out_of_range);
    QVERIFY_EXCEPTION_THROWN(sim.wireState("missing"), std::out_of_range);
}

void SimulatorTest::testUniversalShiftRegister() {
    Simulator sim(parse(TestUtils::readExample("08_univ_shift.v")));

    auto set = [&](const char* name, int v) {
        sim.setInput(name, bit(v));
        sim.evaluate();
    };
    // Rising then falling clock edge; data inputs are already settled (setup time)
    auto clock = [&]() { set("CLK", 1); set("CLK", 0); };
    auto q = [&]() -> QString {
        auto str = [&](const char* n) {
            LogicState s = sim.wireState(n);
            return s == LogicState::LogicHigh ? "1" : s == LogicState::LogicLow ? "0" : "Z";
        };
        return QString(str("Q1")) + str("Q0");
    };

    for (const char* in : {"CLK", "RST", "S0", "S1", "LIN", "RIN", "I0", "I1"}) set(in, 0);
    QCOMPARE(q(), QString("ZZ")); // No reset yet: stored state is unknown

    set("RST", 1); QCOMPARE(q(), QString("00"));
    set("RST", 0); QCOMPARE(q(), QString("00"));

    // 11 = parallel load
    set("S1", 1); set("S0", 1); set("I1", 1); set("I0", 0);
    clock(); QCOMPARE(q(), QString("10"));

    // 00 = hold
    set("S1", 0); set("S0", 0); set("I1", 0);
    clock(); QCOMPARE(q(), QString("10"));

    // 01 = shift-right: Q1 <- Q0, Q0 <- RIN
    set("S0", 1); set("RIN", 1);
    clock(); QCOMPARE(q(), QString("01"));
    set("RIN", 0);
    clock(); QCOMPARE(q(), QString("10"));

    // 10 = shift-left: Q1 <- LIN, Q0 <- Q1
    set("S0", 0); set("S1", 1); set("LIN", 1);
    clock(); QCOMPARE(q(), QString("11"));
    set("LIN", 0);
    clock(); QCOMPARE(q(), QString("01"));

    // Outputs change only on the rising edge, not while CLK stays low
    set("LIN", 1); QCOMPARE(q(), QString("01"));

    // Asynchronous reset with the clock high
    set("CLK", 1); QCOMPARE(q(), QString("10"));
    set("RST", 1); QCOMPARE(q(), QString("00"));
}

QTEST_MAIN(SimulatorTest)
#include "simulatorTest.moc"
