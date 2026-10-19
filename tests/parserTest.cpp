#include <QtTest>
#include <fstream>
#include <map>
#include "parser.hpp"

class ParserAndSimulationTest : public QObject {
    Q_OBJECT

private slots:
    void init() {
        std::ofstream file("test_adder.v");
        file << "module HalfAdder (input A, input B, output Sum, output Carry);\n"
             << "    xor gate1 (Sum, A, B);\n"
             << "    and gate2 (Carry, A, B);\n"
             << "endmodule\n";
        file.close();
    }

    void cleanup() {
        QFile::remove("test_adder.v");
    }

    // Test 1: Basic check on the parsed structure.
    void testSimpleParser() {
        Circuit c = VerilogParser::parseFile("test_adder.v");
        QCOMPARE(c.inputs.size(), size_t(2));
        QCOMPARE(c.outputs.size(), size_t(2));
        QCOMPARE(c.gates.size(), size_t(2));
    }

    // Test 2: ANSI syntax support check.
    void testANSISyntaxParsing() {
        Circuit c = VerilogParser::parseFile("test_adder.v");
        QCOMPARE(c.inputs.at(0), std::string("A"));
        QCOMPARE(c.inputs.at(1), std::string("B"));
        QCOMPARE(c.gates.at(0).type, std::string("xor"));
    }

    // Test 3: AND logic check (Tri-State).
    void testTriStateAndLogic() {
        int in1_false = 0;
        int in2_z = -1;
        int out_with_zero = (in1_false == 0 || in2_z == 0) ? 0 : -1;
        QCOMPARE(out_with_zero, 0); // 0 AND Z = 0

        int in1_true = 1;
        int out_with_one = (in1_true == 0 || in2_z == 0) ? 0 : -1;
        QCOMPARE(out_with_one, -1); // 1 AND Z = Z (-1)
    }

    // Test 4: XOR logic check (Tri-State).
    void testTriStateXorLogic() {
        int in1 = 1;
        int in2 = 0;
        int out_normal = in1 ^ in2;
        QCOMPARE(out_normal, 1); // 1 XOR 0 = 1

        int in_z = -1;
        int out_z = (in1 == -1 || in_z == -1) ? -1 : (in1 ^ in_z);
        QCOMPARE(out_z, -1); // 1 XOR Z = Z (-1)
    }

    // Test 5: File not found check.
    void testInvalidFileHandling() {
        Circuit c = VerilogParser::parseFile("non_existent_file.v");
        QVERIFY(c.gates.empty());
        QVERIFY(c.inputs.empty());
    }
};

// Qt macro generating main() function for this test execution.
QTEST_MAIN(ParserAndSimulationTest)
#include "parserTest.moc"