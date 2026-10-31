#include <QtTest>
#include "../include/parser.hpp"

class ParserTest : public QObject {
    Q_OBJECT

private slots:
    void testNewGatesParsing();
    void testTriStateLogicPropagation();
    void testSyntaxErrorPositionTracking();
    void testTruncatedGateDoesNotCrash();
    void testCommentsAreIgnored();
    void testAssignExpressions();
    void testVectorsAndBitSelects();
    void testCommentedDesignMatchesClean();
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
    QCOMPARE(parser.getInputs().size(), 2U);
    QCOMPARE(parser.getOutputs().size(), 5U);
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
        QFAIL("Expected SyntaxException");
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
        QFAIL("Expected SyntaxException");
    } catch (const MiniEDA::SyntaxException& ex) {
        QCOMPARE(ex.line(), 2U);
        QCOMPARE(ex.rawMessage(), std::string("Anonymous gate instantiation block discovered. Gate name required"));
    }
}

void ParserTest::testTruncatedGateDoesNotCrash() {
    MiniEDA::Parser parser;
    // Input ends right after the gate instance name, no pin list
    std::string truncated =
        "module m (input a, output b);\n"
        "  and g1";

    QVERIFY_EXCEPTION_THROWN(parser.parseNetlist(truncated), MiniEDA::SyntaxException);
}

void ParserTest::testCommentsAreIgnored() {
    MiniEDA::Parser parser;
    std::string verilog =
        "// header comment with and, or keywords\n"
        "module m (\n"
        "  // 2 distinct inputs\n"
        "  input a, b,\n"
        "  /* block\n"
        "     comment */ output y\n"
        ");\n"
        "  and g1 (y, a, b); // trailing comment\n"
        "  /* unterminated";

    QVERIFY(parser.parseNetlist(verilog));
    QCOMPARE(parser.getInputs().size(), 2U);
    QCOMPARE(parser.getOutputs().size(), 1U);
    QCOMPARE(parser.getOutputs().at(0), std::string("y"));
    QCOMPARE(parser.getGates().size(), 1U);

    // Line tracking must survive block comments spanning multiple lines
    std::string bad =
        "module m (input a, output y);\n"
        "  /* line 2\n"
        "     line 3 */\n"
        "  wire w\n"
        "  and g1 (y, a, w);\n"
        "endmodule";
    try {
        parser.parseNetlist(bad);
        QFAIL("Expected SyntaxException");
    } catch (const MiniEDA::SyntaxException& ex) {
        QCOMPARE(ex.line(), 4U);
        QCOMPARE(ex.column(), 3U);
    }
}

void ParserTest::testAssignExpressions() {
    using MiniEDA::LogicState;
    MiniEDA::Parser parser;
    std::string verilog =
        "module Dataflow (input a, b, c, d, output y, z, x, w, v);\n"
        "  wire t;\n"
        "  assign t = a;\n"                      // plain net assignment -> buffer
        "  assign y = ~(a & b) | c ^ d;\n"       // precedence: ~() then ^ then |
        "  assign x = a ~^ b ~^ c;\n"            // chained xnor: (a xnor b) xnor c
        "  assign w = a && b || !c;\n"
        "  assign v = t & b & c;\n"              // same operator chain -> one 3-input AND
        "endmodule";

    QVERIFY(parser.parseNetlist(verilog));
    QCOMPARE(parser.getInputs().size(), 4U);
    QCOMPARE(parser.getOutputs().size(), 5U);

    const auto& gates = parser.getGates();
    QCOMPARE(gates.at(0).type, MiniEDA::GateType::BUF);
    QCOMPARE(gates.at(0).outputWire, std::string("t"));
    QCOMPARE(gates.back().type, MiniEDA::GateType::AND);
    QCOMPARE(gates.back().outputWire, std::string("v"));
    QCOMPARE(gates.back().inputWires.size(), 3U);

    parser.setInputState("a", LogicState::LogicHigh);
    parser.setInputState("b", LogicState::LogicLow);
    parser.setInputState("c", LogicState::LogicLow);
    parser.setInputState("d", LogicState::LogicHigh);
    parser.evaluateCircuit();

    QCOMPARE(parser.getWireState("t"), LogicState::LogicHigh);
    QCOMPARE(parser.getWireState("y"), LogicState::LogicHigh); // ~(1&0)=1 | (0^1)=1
    QCOMPARE(parser.getWireState("x"), LogicState::LogicHigh); // (1 xnor 0)=0, 0 xnor 0 = 1
    QCOMPARE(parser.getWireState("w"), LogicState::LogicHigh); // (1&&0)=0 || !0=1
    QCOMPARE(parser.getWireState("v"), LogicState::LogicLow);  // 1&0&0

    QVERIFY_EXCEPTION_THROWN(parser.parseNetlist("module m (input a, output y);\n  assign y = a\nendmodule"),
                             MiniEDA::SyntaxException);
    QVERIFY_EXCEPTION_THROWN(parser.parseNetlist("module m (input a, b, output y);\n  assign y = (a & b;\nendmodule"),
                             MiniEDA::SyntaxException);
    QVERIFY_EXCEPTION_THROWN(parser.parseNetlist("module m (input a, b, output y);\n  assign y = a & ;\nendmodule"),
                             MiniEDA::SyntaxException);
}

void ParserTest::testVectorsAndBitSelects() {
    using MiniEDA::LogicState;
    MiniEDA::Parser parser;
    std::string verilog =
        "module dataflow_logic (\n"
        "    input [1:0] a,\n"
        "    input b,\n"
        "    output y,\n"
        "    output z\n"
        ");\n"
        "    wire [0:0] n;\n"
        "    assign y = (a[0] & b) | (~a[1]);\n"
        "    assign z = (a[0] ^ b) & y;\n"
        "    not g1 (n[0], a[1]);\n"
        "endmodule";

    QVERIFY(parser.parseNetlist(verilog));
    QCOMPARE(parser.getInputs().size(), 3U);
    QCOMPARE(parser.getInputs().at(0), std::string("a[1]"));
    QCOMPARE(parser.getInputs().at(1), std::string("a[0]"));
    QCOMPARE(parser.getInputs().at(2), std::string("b"));

    parser.setInputState("a[0]", LogicState::LogicHigh);
    parser.setInputState("a[1]", LogicState::LogicLow);
    parser.setInputState("b", LogicState::LogicHigh);
    parser.evaluateCircuit();

    QCOMPARE(parser.getWireState("y"), LogicState::LogicHigh); // (1&1) | ~0
    QCOMPARE(parser.getWireState("z"), LogicState::LogicLow);  // (1^1) & 1
    QCOMPARE(parser.getWireState("n[0]"), LogicState::LogicHigh);

    // Bit select outside the declared range
    QVERIFY_EXCEPTION_THROWN(parser.parseNetlist("module m (input [1:0] a, output y);\n  assign y = a[2];\nendmodule"),
                             MiniEDA::SyntaxException);
    // Whole-vector operand is not a 1-bit net
    QVERIFY_EXCEPTION_THROWN(parser.parseNetlist("module m (input [1:0] a, output y);\n  assign y = a;\nendmodule"),
                             MiniEDA::SyntaxException);
    // Malformed range
    QVERIFY_EXCEPTION_THROWN(parser.parseNetlist("module m (input [1 0] a, output y);\nendmodule"),
                             MiniEDA::SyntaxException);
}

void ParserTest::testCommentedDesignMatchesClean() {
    using MiniEDA::LogicState;

    // Same circuit twice: once clean, once full of comments containing keywords and valid-looking statements
    std::string clean =
        "module half_adder_ext (input a, b, en, output sum, carry, flag);\n"
        "  wire s0, c0;\n"
        "  xor x1 (s0, a, b);\n"
        "  and a1 (c0, a, b);\n"
        "  assign sum = s0 & en;\n"
        "  assign carry = c0 & en;\n"
        "  nor n1 (flag, s0, c0);\n"
        "endmodule";

    std::string commented =
        "// module fake (input q, output r); and g0 (r, q, q); endmodule\n"
        "/* input ghost_in; output ghost_out;\n"
        "   wire ghost_w; assign ghost_out = ghost_in | ghost_w;\n"
        "   nand ghost_g (ghost_out, ghost_in, ghost_w); */\n"
        "module half_adder_ext ( // ports: input x, output y\n"
        "  input a, b, /* input c, */ en,\n"
        "  // output never_declared,\n"
        "  output sum, carry, /* and, or, xor */ flag\n"
        ");\n"
        "  wire s0, /* wire bogus; */ c0; // wire another_bogus;\n"
        "  xor x1 (s0, a, /* c, */ b); // xor x2 (s0, a, a);\n"
        "  and a1 (c0, a, b); /* not n9 (c0, a); */\n"
        "  // assign sum = a | b;\n"
        "  assign sum = s0 /* | a */ & en;\n"
        "  assign carry = c0 & en; // ; ) ( stray delimiters\n"
        "  /* // a line comment marker inside a block comment */\n"
        "  // /* a block opener inside a line comment does not start a block\n"
        "  nor n1 (flag, s0, c0);\n"
        "/**/ endmodule /* trailing ***/";

    MiniEDA::Parser cleanParser, commentedParser;
    QVERIFY(cleanParser.parseNetlist(clean));
    QVERIFY(commentedParser.parseNetlist(commented));

    // Structure must be identical: no ports, wires or gates leaking out of comments
    QCOMPARE(commentedParser.getInputs(), cleanParser.getInputs());
    QCOMPARE(commentedParser.getOutputs(), cleanParser.getOutputs());
    QCOMPARE(commentedParser.getGates().size(), cleanParser.getGates().size());
    for (size_t g = 0; g < cleanParser.getGates().size(); ++g) {
        const auto& expected = cleanParser.getGates().at(g);
        const auto& actual = commentedParser.getGates().at(g);
        // Generated assign gate names embed the source line, which differs between the two versions
        if (expected.name.rfind("assign_L", 0) != 0) {
            QCOMPARE(actual.name, expected.name);
        }
        QCOMPARE(actual.type, expected.type);
        QCOMPARE(actual.outputWire, expected.outputWire);
        QCOMPARE(actual.inputWires, expected.inputWires);
    }

    // Behaviour must be identical for every input combination
    const char* ins[] = {"a", "b", "en"};
    const char* outs[] = {"sum", "carry", "flag"};
    for (int combo = 0; combo < 8; ++combo) {
        for (int k = 0; k < 3; ++k) {
            LogicState s = (combo >> k) & 1 ? LogicState::LogicHigh : LogicState::LogicLow;
            cleanParser.setInputState(ins[k], s);
            commentedParser.setInputState(ins[k], s);
        }
        cleanParser.evaluateCircuit();
        commentedParser.evaluateCircuit();
        for (const char* out : outs) {
            QCOMPARE(commentedParser.getWireState(out), cleanParser.getWireState(out));
        }
    }

    // Spot-check real values: a=1, b=1, en=1 -> sum 0, carry 1, flag 0
    for (const char* in : ins) commentedParser.setInputState(in, LogicState::LogicHigh);
    commentedParser.evaluateCircuit();
    QCOMPARE(commentedParser.getWireState("sum"), LogicState::LogicLow);
    QCOMPARE(commentedParser.getWireState("carry"), LogicState::LogicHigh);
    QCOMPARE(commentedParser.getWireState("flag"), LogicState::LogicLow);
}

QTEST_MAIN(ParserTest)
#include "parserTest.moc"