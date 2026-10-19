#include "parser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

std::string VerilogParser::cleanToken(const std::string& token) {
    std::string cleaned = token;
    cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), [](char c) {
        return c == ';' || c == ',' || c == '(' || c == ')' || c == '\r' || c == '\n';
    }), cleaned.end());
    return cleaned;
}

Circuit VerilogParser::parseFile(const std::string& filePath) {
    Circuit circuit;
    std::ifstream file(filePath);
    if (!file.is_open()) return circuit;

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::string content = buffer.str();
    std::string normalizedContent = "";

    for (char c : content) {
        if (c == '(' || c == ')' || c == ',' || c == ';') {
            normalizedContent += " ";
            normalizedContent += c;
            normalizedContent += " ";
        } else {
            normalizedContent += c;
        }
    }

    std::stringstream ss(normalizedContent);
    std::string token;

    while (ss >> token) {
        if (token == "input") {
            std::string nextToken;
            if (ss >> nextToken) {
                std::string cleaned = cleanToken(nextToken);
                if (!cleaned.empty()) circuit.inputs.push_back(cleaned);
            }
        } else if (token == "output") {
            std::string nextToken;
            if (ss >> nextToken) {
                std::string cleaned = cleanToken(nextToken);
                if (!cleaned.empty()) circuit.outputs.push_back(cleaned);
            }
        } else if (token == "wire") {
            std::string nextToken;
            while (ss >> nextToken) {
                if (nextToken == ";") break;
                std::string cleaned = cleanToken(nextToken);
                if (!cleaned.empty()) circuit.wires.push_back(cleaned);
            }
        } else if (token == "and" || token == "xor" || token == "or") {
            Gate gate;
            gate.type = token;
            ss >> gate.name;
            gate.name = cleanToken(gate.name);

            // Verilog syntax: gate_type name (output, input1, input2);
            std::string openParen;
            ss >> openParen; // consume '('

            std::string pin;
            if (ss >> pin) gate.output = cleanToken(pin); // Always the first pin is the output.

            // Inputs are next till the the ending ')'.
            while (ss >> pin) {
                if (pin == ")") break;
                std::string cleanedPin = cleanToken(pin);
                if (!cleanedPin.empty()) {
                    gate.inputs.push_back(cleanedPin);
                }
            }
            circuit.gates.push_back(gate);
        }
    }

    return circuit;
}