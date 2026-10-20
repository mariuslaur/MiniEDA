#include <QtTest>
#include "../include/parser.hpp"

class ParserTest : public QObject {
    Q_OBJECT

private slots:
    void testNewGatesParsing();
    void testTriStateLogicPropagation();
    void testSyntaxErrorPositionTracking();
};

void ParserTest::testNewGatesParsing() {
    MiniEDA::Parser parser;
    std::string verilog = 
        "module TestCircuit (input a, b, output out1, out2, out3, out4, out5);\n"
        "  nand g1 (out1, a, b);\n"
        "  nor  g2 (out2, a, b);\n"
        "  xnor g3 (out3, a, b);\n"
        "  buf  g4 (out4, a);\n"
        "  not  g5 (out5, b);\n"
        "endmodule";

    QVERIFY(parser.parseNetlist(verilog));
    QCOMPARE(parser.getGates().size(), 5U);
    QCOMPARE(parser.getGates().at(0).type, MiniEDA::GateType::NAND);
    QCOMPARE(parser.getGates().at(1).type, MiniEDA::GateType::NOR);
    QCOMPARE(parser.getGates().at(2).type, MiniEDA::GateType::XNOR);
    QCOMPARE(parser.getGates().at(3).type, MiniEDA::GateType::BUF);
    QCOMPARE(parser.getGates().at(4).type, MiniEDA::GateType::NOT);
}

void ParserTest::testTriStateLogicPropagation() {
    MiniEDA::Parser parser;
    std::string verilog = 
        "module FullSet (input A, B, output Y_nand, Y_nor, Y_xnor, Y_buf, Y_not);\n"
        "  nand g1 (Y_nand, A, B);\n"
        "  nor  g2 (Y_nor, A, B);\n"
        "  xnor g3 (Y_xnor, A, B);\n"
        "  buf  g4 (Y_buf, A);\n"
        "  not  g5 (Y_not, B);\n"
        "endmodule";

    QVERIFY(parser.parseNetlist(verilog));

    // 1: Default (Z)
    parser.evaluateCircuit();
    QCOMPARE(parser.getWireState("Y_nand"), MiniEDA::LogicState::HighImpedance);
    QCOMPARE(parser.getWireState("Y_nor"), MiniEDA::LogicState::HighImpedance);

    // 2: A = 1, B = 0
    parser.setInputState("A", MiniEDA::LogicState::LogicHigh);
    parser.setInputState("B", MiniEDA::LogicState::LogicLow);
    parser.evaluateCircuit();

    QCOMPARE(parser.getWireState("Y_nand"), MiniEDA::LogicState::LogicHigh); // 1 NAND 0 = 1
    QCOMPARE(parser.getWireState("Y_nor"), MiniEDA::LogicState::LogicLow);   // 1 NOR 0 = 0
    QCOMPARE(parser.getWireState("Y_xnor"), MiniEDA::LogicState::LogicLow);  // 1 XNOR 0 = 0
    QCOMPARE(parser.getWireState("Y_buf"), MiniEDA::LogicState::LogicHigh);  // BUF 1 = 1
    QCOMPARE(parser.getWireState("Y_not"), MiniEDA::LogicState::LogicHigh);  // NOT 0 = 1
}

void ParserTest::testSyntaxErrorPositionTracking() {
    MiniEDA::Parser parser;
    
    // Scenario A: Missing terminal semicolon on wire declaration (Line 3)
    std::string badVerilogSemicolon = 
        "module ErrorModule (input a, output b);\n"
        "  wire internal_net1, internal_net2\n" // Semicolon missing here
        "  and g1 (b, a, internal_net1);\n"
        "endmodule";

    QVERIFY_EXCEPTION_THROWN(parser.parseNetlist(badVerilogSemicolon), MiniEDA::SyntaxException);
    
    try {
        parser.parseNetlist(badVerilogSemicolon);
    } catch (const MiniEDA::SyntaxException& ex) {
        QCOMPARE(ex.line(), 2U);
        QCOMPARE(ex.column(), 3U); // Starts exactly at the keyword block
    }

    // Scenario B: Anonymous gate initialization structural error
    std::string badAnonymousGate = 
        "module ErrorModule2 (input a, output b);\n"
        "  not (b, a);\n" // Gate instance identifier missing name
        "endmodule";

    try {
        parser.parseNetlist(badAnonymousGate);
    } catch (const MiniEDA::SyntaxException& ex) {
        QCOMPARE(ex.line(), 2U);
        QCOMPARE(ex.rawMessage(), std::string("Anonymous gate instantiation block discovered. Gate name required"));
    }
}

QTEST_MAIN(ParserTest)
#include "parserTest.moc"