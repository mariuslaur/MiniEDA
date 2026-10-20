# MiniEDA: Adaptive Verilog Netlist Parser & Tri-State Visual Simulator

A lightweight, high-performance **EDA (Electronic Design Automation)** desktop application built in C++17 and Qt 6. It parses a subset of structural Verilog netlists, constructs an in-memory directed topology graph, renders an adaptive graphical view of the circuit, and provides interactive, deterministic digital logic simulation.

Designed with a strict separation of concerns (MVC architecture) and built defensively against memory corruption, this project serves as a technical showcase for low-level software toolchain engineering and compiler front-end development.

---

## 🚀 Key Features (Engineering Overview)

- **Deterministic AST & Netlist Parser**: Implements state-based lexical processing directly from an in-memory text buffer, decoupling compilation stages from slow disk I/O operations.
- **Robust Spatial Error Tracking**: Features a custom token-based tracking layout that computes precise line and column metrics, throwing localized `SyntaxException` wrappers for syntax failures.
- **Trivalent Logic Engine**: Simulates authentic hardware states: `0` (Logic Low), `1` (Logic High), and `Z` (High-Impedance/Undefined), fully isolated from the visual presentation layer.
- **Expanded Native Gate Primitives**: Built-in support for standard structural elements: `and`, `or`, `xor`, `nand`, `nor`, `xnor`, alongside unary operations like `buf` and `not`.
- **Adaptive Vector Layout Canvas**: Dynamically recalculates layout geometry and viewport scene bounding boxes based on netlist complexity without relying on brittle hardcoded coordinates.
- **Production-Grade Memory Sandboxing**: Zero raw pointer casts, aggressive `.at()` bounds validation across STL containers, cross-platform `\r` filtering, and clean RAII compliance for crash-free execution.

---

## 🏗️ Architectural Breakdown

```text
[ Verilog Input String ] ──► [ Tokenizer & Lexer ] ──► [ In-Memory Topo Graph ]
                                                              │
[ Interactive Qt Canvas ] ◄── [ Live Tri-State Engine ] ◄─────┘
```

- **include/parser.hpp & src/parser.cpp**: Contains the logical core of the compiler front-end. Implements spatial token collection, strict keyword boundary enforcement, and topological evaluation matrices.
- **include/mainWindow.hpp & src/mainWindow.cpp**: Drives the application controller and presentation layer. Leverages Qt's object tree (Parent-Child tracking) for automatic memory reclamation, preventing heap leaks.
- **tests/parserTest.cpp**: Independent validation suite using `QTest` and CMake `CTest` for boundary state verification, truth tables validation, and negative syntax testing.

---

## 📂 Project Structure

```text
MiniEDA/
├── CMakeLists.txt         # Streamlined modern CMake script with AUTOMOC and CTest config
├── include/               # Clean abstraction layers and type definitions
│   ├── mainWindow.hpp     # UI event map controllers and scene geometry bounds
│   └── parser.hpp         # Node topologies, gate definitions, and exception structures
├── src/                   # Source core execution layer
│   ├── main.cpp           # Application bootstrapper
│   ├── mainWindow.cpp     # Visual layout rendering and signal event slot routing
│   └── parser.cpp         # AST lexical processing and tri-state truth table engine
└── tests/                 # Quality Assurance testing harness
    └── parserTest.cpp     # Multi-scenario verification suite
```

---

## ⚙️ How to Build & Run (Linux / WSL2)

### Prerequisites

Ensure you have your GCC compiler toolchain, CMake build system, and Qt6 graphics dependencies set up correctly:

```bash
sudo apt update
sudo apt install -y build-essential cmake qt6-base-dev qt6-wayland libxcb-cursor0 gdb
```

### Compilation

Build the project defensively using modern CMake wrapper configurations:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Execution

Force X11 platform integration inside WSL2 runtime environments for stable back-end rendering performance:

```bash
QT_QPA_PLATFORM=xcb ./MiniEDA
```

---

## 🧪 Running the Unit Test Suite

The validation harness evaluates key architectural invariants (ANSI parsing stability, token identification, trivalent bitwise logic propagation, and precise error coordinate tracking).

Execute tests directly via the CMake abstraction layer from your `build` directory:

```bash
ctest --output-on-failure
```

---

## 📑 Sample Verilog Format Supported

The engine natively decodes ANSI-style inline module port declarations alongside mixed control structures:

```verilog
module MixedControlUnit (
    input SystemEnable,
    input SignalA,
    input SignalB,
    output Alert,
    output ReadySignal,
    output SafeStatus
);
    wire netInternal;

    nand check1 (netInternal, SignalA, SignalB);
    nor  check2 (Alert, netInternal, SystemEnable);
    xnor check3 (ReadySignal, SignalA, SystemEnable);
    buf  buffer1 (SafeStatus, SystemEnable);
endmodule
```
