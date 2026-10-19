#pragma once
#include <string>
#include <vector>

enum LogicState {
    LOGIC_LOW = 0,
    LOGIC_HIGH = 1,
    LOGIC_UNDEFINED = -1 // Starea Z / High-Impedance
};

struct Gate {
    std::string type; 
    std::string name; 
    std::string output;
    std::vector<std::string> inputs;
};

struct Circuit {
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    std::vector<std::string> wires;
    std::vector<Gate> gates;
};

class VerilogParser {
public:
    static Circuit parseFile(const std::string& filePath);
private:
    static std::string cleanToken(const std::string& token);
};