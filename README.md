# MiniEDA: Adaptive Verilog Netlist Parser & Tri-State Visual Simulator

A lightweight, high-performance **EDA (Electronic Design Automation)** desktop application built in C++17 and Qt 6. It parses a subset of structural Verilog netlists, constructs a directed topology graph in memory, renders an adaptive graphical view of the circuit, and provides interactive, event-driven digital logic simulation.

Designed with a strict separation of concerns (MVC architecture) and built defensively against memory corruption, this project serves as a technical showcase for low-level software toolchain engineering.

---

## Key Features

- **ANSI Verilog Parser**: Syntaxes structural netlists directly from an integrated code editor using a fast text-token processing approach.
- **Tri-State Logic Engine**: Simulates realistic hardware states: `0` (Logic Low), `1` (Logic High), and `Z` (High-Impedance/Undefined).
- **Adaptive Visual Canvas**: Dynamically centers and recalculates layout geometry based on viewport sizing and netlist complexity. No hardcoded coordinates.
- **Interactive Simulation Flow**: Clickable external inputs trigger live bitwise logic propagation through lines and internal gate representations with clear execution feedback.
- **Robust Engineering Standards**: Defensive programming via `.at()` bounds checking, zero raw pointer casts, and zero compiler warnings.

---

## Architectural Breakdown
[ Verilog Code / Editor ] ──► [ Tokenizer & Parser ] ──► [ In-Memory Topo Graph ]
│
[ Interactive Qt Canvas ] ◄── [ Live Tri-State Engine ] ◄─────────┘

- **src/Parser.cpp**: Implements state-based lexical processing and netlist graph construction.
- **src/MainWindow.cpp**: Manages safe snapshot-based scene iteration loops (`QGraphicsScene`), decoupling calculations from graphics rendering elements to prevent thread bottlenecks and UI crashes.
- **tests/ParserTest.cpp**: Independent test suite leveraging `QTest` and CMake `CTest` for structural accuracy and boundary state verification.

---

## Project Structure

```text
MiniEDA/
├── CMakeLists.txt         # Modern, streamlined CMake file with AUTOMOC and CTest config
├── include/               # Clean abstraction layers
│   ├── mainWindow.hpp
│   └── parser.hpp
├── src/                   # Source core execution layer
│   ├── main.cpp
│   ├── mainWindow.cpp
│   └── parser.cpp
└── tests/                 # Quality Assurance and test coverage suite
    └── parserTest.cpp
```

---

## How to Build & Run (Linux / WSL2)

### Prerequisites

Ensure you have your GCC compiler, CMake, and Qt6 dependencies setup correctly:

```bash
sudo apt update
sudo apt install -y build-essential cmake qt6-base-dev qt6-wayland libxcb-cursor0 gdb
```

### Compilation

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Execution

Force X11 platform integration inside WSL environments for stable performance:

```bash
QT_QPA_PLATFORM=xcb ./MiniEDA
```

---

## Running the Unit Test Suite

The project includes an automatic verification harness evaluating 5 critical test cases (syntactic token identification, ANSI parsing stability, trivalent bit logic truth tables, and out-of-bounds error handling).

Run them cleanly via CMake's wrapper from your `build` directory:

```bash
ctest --output-on-failure
```

---

## Sample Verilog Format Supported

You can write or load code structures directly inside the app's IDE editor pane:

```verilog
module ExtendedAdder (input A, input B, input C, output Sum, output Carry, output OC);
    wire net1, net2;
    xor gate1 (Sum, A, B);
    and gate2 (Carry, A, B);
    and gate3 (OC, A, C);
endmodule
```
