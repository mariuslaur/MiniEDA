#include <QtTest>
#include <QDir>
#include "testUtils.hpp"

using MiniEDA::GateType;
using MiniEDA::LogicState;
using MiniEDA::SyntaxException;
using TestUtils::parse;

class ParserTest : public QObject {
    Q_OBJECT

private slots:
    void testNewGatesParsing();
    void testSyntaxErrorPositionTracking();
    void testTruncatedGateDoesNotCrash();
    void testCommentsAreIgnored();
    void testAssignExpressions();
    void testVectorsAndBitSelects();
    void testCommentedDesignMatchesClean();
    void testModuleNameAndHeaderComments();
    void testGateLikeCommentsInMux();
    void testExampleFilesParse();
};

void ParserTest::testNewGatesParsing() {
    MiniEDA::Netlist netlist = parse(
        "module TestCircuit (input a, b, output out1, out2, out3, out4, out5);\n"
        "  nand g1 (out1, a, b);\n"
        "  nor  g2 (out2, a, b);\n"
        "  xnor g3 (out3, a, b);\n"
        "  buf  g4 (out4, a);\n"
        "  not  g5 (out5, b);\n"
        "endmodule");

    QCOMPARE(netlist.inputs.size(), 2U);
    QCOMPARE(netlist.outputs.size(), 5U);
    QCOMPARE(netlist.gates.size(), 5U);
    QCOMPARE(netlist.gates.at(0).type, GateType::NAND);
    QCOMPARE(netlist.gates.at(1).type, GateType::NOR);
    QCOMPARE(netlist.gates.at(2).type, GateType::XNOR);
    QCOMPARE(netlist.gates.at(3).type, GateType::BUF);
    QCOMPARE(netlist.gates.at(4).type, GateType::NOT);
}

void ParserTest::testSyntaxErrorPositionTracking() {
    // Scenario A: Missing terminal semicolon on wire declaration
    std::string badVerilogSemicolon =
        "module ErrorModule (input a, output b);\n"
        "  wire internal_net1, internal_net2\n" // Semicolon missing here
        "  and g1 (b, a, internal_net1);\n"
        "endmodule";

    try {
        parse(badVerilogSemicolon);
        QFAIL("Expected SyntaxException");
    } catch (const SyntaxException& ex) {
        QCOMPARE(ex.line(), 2U);
        QCOMPARE(ex.column(), 3U); // Starts exactly at the keyword block
    }

    // Scenario B: Anonymous gate initialization structural error
    std::string badAnonymousGate =
        "module ErrorModule2 (input a, output b);\n"
        "  not (b, a);\n" // Gate instance identifier missing name
        "endmodule";

    try {
        parse(badAnonymousGate);
        QFAIL("Expected SyntaxException");
    } catch (const SyntaxException& ex) {
        QCOMPARE(ex.line(), 2U);
        QCOMPARE(ex.rawMessage(), std::string("Anonymous gate instantiation block discovered. Gate name required"));
    }
}

void ParserTest::testTruncatedGateDoesNotCrash() {
    // Input ends right after the gate instance name, no pin list
    QVERIFY_EXCEPTION_THROWN(parse("module m (input a, output b);\n  and g1"), SyntaxException);
}

void ParserTest::testCommentsAreIgnored() {
    MiniEDA::Netlist netlist = parse(
        "// header comment with and, or keywords\n"
        "module m (\n"
        "  // 2 distinct inputs\n"
        "  input a, b,\n"
        "  /* block\n"
        "     comment */ output y\n"
        ");\n"
        "  and g1 (y, a, b); // trailing comment\n"
        "  /* unterminated");

    QCOMPARE(netlist.inputs.size(), 2U);
    QCOMPARE(netlist.outputs, (std::vector<std::string>{"y"}));
    QCOMPARE(netlist.gates.size(), 1U);

    // Line tracking must survive block comments spanning multiple lines
    std::string bad =
        "module m (input a, output y);\n"
        "  /* line 2\n"
        "     line 3 */\n"
        "  wire w\n"
        "  and g1 (y, a, w);\n"
        "endmodule";
    try {
        parse(bad);
        QFAIL("Expected SyntaxException");
    } catch (const SyntaxException& ex) {
        QCOMPARE(ex.line(), 4U);
        QCOMPARE(ex.column(), 3U);
    }
}

void ParserTest::testAssignExpressions() {
    MiniEDA::Netlist netlist = parse(
        "module Dataflow (input a, b, c, d, output y, z, x, w, v);\n"
        "  wire t;\n"
        "  assign t = a;\n"                      // plain net assignment -> buffer
        "  assign y = ~(a & b) | c ^ d;\n"       // precedence: ~() then ^ then |
        "  assign x = a ~^ b ~^ c;\n"            // chained xnor: (a xnor b) xnor c
        "  assign w = a && b || !c;\n"
        "  assign v = t & b & c;\n"              // same operator chain -> one 3-input AND
        "endmodule");

    QCOMPARE(netlist.inputs.size(), 4U);
    QCOMPARE(netlist.outputs.size(), 5U);

    const auto& gates = netlist.gates;
    QCOMPARE(gates.at(0).type, GateType::BUF);
    QCOMPARE(gates.at(0).outputWire, std::string("t"));
    QCOMPARE(gates.back().type, GateType::AND);
    QCOMPARE(gates.back().outputWire, std::string("v"));
    QCOMPARE(gates.back().inputWires.size(), 3U);

    QVERIFY_EXCEPTION_THROWN(parse("module m (input a, output y);\n  assign y = a\nendmodule"), SyntaxException);
    QVERIFY_EXCEPTION_THROWN(parse("module m (input a, b, output y);\n  assign y = (a & b;\nendmodule"), SyntaxException);
    QVERIFY_EXCEPTION_THROWN(parse("module m (input a, b, output y);\n  assign y = a & ;\nendmodule"), SyntaxException);
}

void ParserTest::testVectorsAndBitSelects() {
    MiniEDA::Netlist netlist = parse(
        "module dataflow_logic (\n"
        "    input [1:0] a,\n"
        "    input b,\n"
        "    output y\n"
        ");\n"
        "    wire [0:0] n;\n"
        "    not g1 (n[0], a[1]);\n"
        "    and g2 (y, a[0], n[0]);\n"
        "endmodule");

    QCOMPARE(netlist.inputs, (std::vector<std::string>{"a[1]", "a[0]", "b"}));
    QCOMPARE(netlist.wires, (std::vector<std::string>{"n[0]"}));
    QCOMPARE(netlist.gates.at(1).inputWires, (std::vector<std::string>{"a[0]", "n[0]"}));

    // Bit select outside the declared range
    QVERIFY_EXCEPTION_THROWN(parse("module m (input [1:0] a, output y);\n  assign y = a[2];\nendmodule"), SyntaxException);
    // Whole-vector operand is not a 1-bit net
    QVERIFY_EXCEPTION_THROWN(parse("module m (input [1:0] a, output y);\n  assign y = a;\nendmodule"), SyntaxException);
    // Malformed range
    QVERIFY_EXCEPTION_THROWN(parse("module m (input [1 0] a, output y);\nendmodule"), SyntaxException);
}

void ParserTest::testCommentedDesignMatchesClean() {
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

    MiniEDA::Netlist expected = parse(clean);
    MiniEDA::Netlist actual = parse(commented);

    // Structure must be identical: no ports, wires or gates leaking out of comments
    QCOMPARE(actual.inputs, expected.inputs);
    QCOMPARE(actual.outputs, expected.outputs);
    QCOMPARE(actual.wires, expected.wires);
    QCOMPARE(actual.gates.size(), expected.gates.size());
    for (size_t g = 0; g < expected.gates.size(); ++g) {
        const auto& e = expected.gates.at(g);
        const auto& a = actual.gates.at(g);
        // Generated assign gate names embed the source line, which differs between the two versions
        if (e.name.rfind("assign_L", 0) != 0) {
            QCOMPARE(a.name, e.name);
        }
        QCOMPARE(a.type, e.type);
        QCOMPARE(a.outputWire, e.outputWire);
        QCOMPARE(a.inputWires, e.inputWires);
    }

    // Behaviour must be identical for every input combination
    MiniEDA::Simulator cleanSim(expected), commentedSim(actual);
    const char* ins[] = {"a", "b", "en"};
    for (int combo = 0; combo < 8; ++combo) {
        for (int k = 0; k < 3; ++k) {
            cleanSim.setInput(ins[k], TestUtils::bit((combo >> k) & 1));
            commentedSim.setInput(ins[k], TestUtils::bit((combo >> k) & 1));
        }
        cleanSim.evaluate();
        commentedSim.evaluate();
        for (const char* out : {"sum", "carry", "flag"}) {
            QCOMPARE(commentedSim.wireState(out), cleanSim.wireState(out));
        }
    }
}

void ParserTest::testModuleNameAndHeaderComments() {
    // Regression (verilog/06_decoder_2to4.v): "input (" inside a header comment used to create a "(" input
    MiniEDA::Netlist netlist = parse(
        "module Decoder2to4 (\r\n"
        "    input E,      // Enable input (must be 1 for outputs to activate)\r\n"
        "    input A0,     // Selection bit 0 (LSB)\r\n"
        "    output Y0     // Output line 0\r\n"
        ");\r\n"
        "    wire A0_not;\r\n"
        "    not inv0 (A0_not, A0);\r\n"
        "    // Y0 = E & ~A0\r\n"
        "    and gate0 (Y0, E, A0_not);\r\n"
        "endmodule\r\n");

    QCOMPARE(netlist.moduleName, std::string("Decoder2to4"));
    QCOMPARE(netlist.inputs, (std::vector<std::string>{"E", "A0"}));
    QCOMPARE(netlist.outputs, (std::vector<std::string>{"Y0"}));
    QCOMPARE(netlist.gates.size(), 2U);

    // One parser instance can be reused; nothing leaks from the previous parse
    MiniEDA::Parser parser;
    parser.parse("module First (input a, output y);\n  buf b1 (y, a);\nendmodule");
    MiniEDA::Netlist second = parser.parse("module Other (input c, output z);\n  not n1 (z, c);\nendmodule");
    QCOMPARE(second.moduleName, std::string("Other"));
    QCOMPARE(second.inputs, (std::vector<std::string>{"c"}));
    QCOMPARE(second.gates.size(), 1U);

    // Missing module name
    QVERIFY_EXCEPTION_THROWN(parse("module (input a, output y);\nendmodule"), SyntaxException);
}

void ParserTest::testGateLikeCommentsInMux() {
    // Regression (verilog/07_mux_4to1.v): "... S1=0 and S0=0 (selects D0)" in a comment looks like
    // a gate instantiation and used to fail with "Missing ... ';'" at line 25
    MiniEDA::Netlist netlist = parse(TestUtils::readExample("07_mux_4to1.v"));
    QCOMPARE(netlist.moduleName, std::string("Mux4to1"));
    QCOMPARE(netlist.inputs.size(), 6U);
    QCOMPARE(netlist.outputs.size(), 1U);
    QCOMPARE(netlist.gates.size(), 7U);
}

void ParserTest::testExampleFilesParse() {
    QDir dir(MINIEDA_VERILOG_DIR);
    const QStringList files = dir.entryList({"*.v"}, QDir::Files, QDir::Name);
    QVERIFY(!files.isEmpty());
    for (const QString& file : files) {
        MiniEDA::Netlist netlist;
        try {
            netlist = parse(TestUtils::readExample(file.toStdString()));
        } catch (const SyntaxException& ex) {
            QFAIL(qPrintable(file + ": " + QString::fromStdString(ex.what())));
        }
        QVERIFY2(!netlist.moduleName.empty(), qPrintable(file));
        QVERIFY2(!netlist.gates.empty(), qPrintable(file));
    }
}

QTEST_MAIN(ParserTest)
#include "parserTest.moc"
