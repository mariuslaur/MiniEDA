#pragma once
#include "parser.hpp"
#include "simulator.hpp"
#include <fstream>
#include <sstream>
#include <string>

namespace TestUtils {

// Reads a design from the project's verilog/ folder (path injected by CMake)
inline std::string readExample(const std::string& fileName) {
    std::ifstream file(std::string(MINIEDA_VERILOG_DIR) + "/" + fileName);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

inline MiniEDA::Netlist parse(const std::string& verilog) {
    return MiniEDA::Parser().parse(verilog);
}

inline MiniEDA::LogicState bit(int value) {
    return value ? MiniEDA::LogicState::LogicHigh : MiniEDA::LogicState::LogicLow;
}

} // namespace TestUtils
