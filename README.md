# MiniEDA: Adaptive Verilog Netlist Parser & Tri-State Visual Simulator

A lightweight **EDA (Electronic Design Automation)** desktop application built in C++17 and Qt 6. It parses a subset of Verilog (structural gate netlists and 1-bit dataflow `assign` statements), builds an in-memory gate netlist, renders an auto-routed schematic of the circuit, and provides interactive tri-state logic simulation, including sequential circuits built from gates (latches, flip-flops, shift registers).

Designed with a strict separation between the parsing/simulation core and the Qt presentation layer, this project serves as a technical showcase for low-level software toolchain engineering and compiler front-end development.

---

## 🚀 Key Features (Engineering Overview)

- **In-Memory Netlist Parser**: Tokenizes and parses Verilog directly from an in-memory text buffer into a gate netlist, decoupling compilation stages from disk I/O.
- **Robust Spatial Error Tracking**: Every token carries its line and column, so syntax failures throw a localized `SyntaxException` pointing at the exact position, including after multi-line comments.
- **Comment-Aware Lexer**: `//` line comments and `/* */` block comments are stripped during tokenization, so keywords inside comments (`input`, `and`, `assign`, ...) never leak into the design.
- **Gate Primitives**: Built-in support for `and`, `or`, `xor`, `nand`, `nor`, `xnor`, `buf` and `not`, with any number of inputs.
- **Dataflow `assign` Lowering**: Boolean expressions using `~ ! & && ^ ~^ ^~ | ||` and parentheses (standard Verilog precedence) are lowered into gates; chains of the same operator become a single multi-input gate.
- **Vectors & Bit Selects**: Declarations like `input [1:0] a` are expanded into scalar nets `a[1]`, `a[0]`; bit selects (`a[0]`) work in expressions and gate pin lists, with range validation.
- **Trivalent Logic Engine**: Simulates `0` (Logic Low), `1` (Logic High) and `Z` (High-Impedance/Undefined). Controlling values resolve outputs even when other inputs are `Z` (a `0` on AND/NAND, a `1` on OR/NOR), which lets cross-coupled feedback loops such as latches and master-slave flip-flops settle into defined states.
- **Auto-Routed Schematic**: Inputs, gates and outputs are laid out in columns; every net gets its own dedicated routing track, so wires of different nets never overlap (only perpendicular crossings). The module frame grows with the number of tracks, and every box is sized to fit the largest label in its category.
- **Zoomable Canvas**: Ctrl + mouse wheel zoom, fit-to-window, 100% view and drag-to-pan.
- **Defensive Memory Handling**: `.at()` bounds validation across STL containers, cross-platform `\r` filtering, and RAII / Qt parent-child ownership for crash-free execution.

---

## 🏗️ Architectural Breakdown

```text
[ Verilog Input String ] ──► [ Lexer ] ──► [ Parser ] ──► [ Netlist ] ──► [ Simulator ]
                                                              │                 │
                                                              ▼                 ▼
                                                   [ Schematic Layout ] ──► [ Schematic Scene ] ◄── clicks
```

The code is split into three layers, each built as its own CMake target:

| Layer | Target | Depends on | Responsibility |
|---|---|---|---|
| Core | `MiniEDACore` | C++ standard library only | `Lexer` (tokens with line/column, comment stripping), `Parser` (declarations, gates, vectors, `assign` lowering into a `Netlist`), `Simulator` (tri-state evaluation of a `Netlist`) |
| Layout | `MiniEDALayout` | Core + Qt Core | `computeSchematicLayout`: placement of ports and gates and orthogonal channel routing, as pure geometry (`QPointF`/`QRectF`), no scene or widgets |
| UI | `MiniEDAUi` (+ `MiniEDA` app) | Layout + Qt Widgets | Graphics items, the schematic scene, the zoomable view and the main window; a library so the UI tests can link it |

- The **`Netlist`** is plain data (module name, ports, wires, gates) shared by the simulator and the layout.
- The **`Simulator`** owns the net states; the parser holds no simulation state.
- The **`SchematicScene`** keeps direct pointers to every wire, gate and port item, so a refresh after each click re-colours them without searching the scene.
- The **`MainWindow`** only wires the widgets together: load a design, forward input clicks to the simulator, refresh the scene.

---

## 🧠 Algorithms & Design Patterns

### Algorithms

**Lexer** (`src/lexer.cpp`)
- Single-pass character scanner with line/column tracking; comments and `\r` are skipped while scanning.
- Two-character operators (`~^`, `&&`, `||`) are recognized in the parser by checking that the two tokens are adjacent in the source, a light form of the *maximal munch* rule.

**Parser** (`src/parser.cpp`)
- **Recursive descent**, one function per statement type (`parseModuleHeader`, `parseDeclaration`, `parseGateInstance`, `parseAssign`).
- **Expressions**: one function per precedence level (`parseOrExpr` → `parseXorExpr` → `parseAndExpr` → `parseUnaryExpr`), the classic way to encode operator precedence in a recursive descent parser.
- **Expression → gates lowering**: each operator becomes a gate driving a temporary net (`$n1`, `$n2`, ...), similar to three-address code in a compiler. Chains of the same associative operator are merged into one n-input gate, and the root gate drives the target net directly (no extra buffer).
- **Bit-blasting**: `[msb:lsb]` vectors are expanded into scalar nets (`a[1]`, `a[0]`).

**Simulator** (`src/simulator.cpp`)
- **Iterative fixed-point evaluation**: all gates are evaluated repeatedly until no net changes, capped at 100 passes so oscillating loops terminate. Updates are visible immediately within the same pass (Gauss-Seidel style).
- **Three-valued logic with controlling values**: a `0` on AND/NAND or a `1` on OR/NOR decides the output even when other inputs are `Z`, the same way Verilog simulators treat X/Z on gate primitives. This is what lets latches and flip-flops settle.
- It is **not event-driven**: every pass evaluates every gate, which is fine at this size.

**Layout & routing** (`src/schematicLayout.cpp`)
- **Placement**: three fixed columns (inputs, gates, outputs), each centered vertically in the module frame.
- **Channel routing with dedicated tracks**: every net gets its own vertical track in each routing channel, so wires of different nets can never overlap (only cross at right angles).
- **Bypass gutters**: gate-to-gate wires leave the gate on the right, go around the gate column on their own horizontal lane (above or below, chosen by the average position of the connected gates), and enter the destination gate from the left.
- **Nudging**: a port is shifted by ±10/20 px when its wire would run on the same line as a wire of another net.

### Design Patterns

| Pattern | Where |
|---|---|
| **Layered architecture** | Core (no Qt) → Layout (Qt Core) → UI (Widgets); each layer depends only on the ones below and is a separate CMake target. |
| **Functional core, imperative shell** | `computeSchematicLayout` is a pure function (netlist + sizes → geometry); all Qt state lives in `SchematicScene`, so the layout is testable without a GUI. |
| **Model–View–Controller** | Model: `Netlist` + `Simulator`. View: `SchematicScene` and its items. Controller: `MainWindow` (load, forward clicks, refresh). |
| **Observer** | Qt signals/slots (`inputToggled`, `zoomChanged`) and the `onToggled` callback of `InputPortItem`. |
| **Composite** | Qt's `QGraphicsItem` tree: labels are children of their boxes and move and get deleted with them. |
| **Data Transfer Objects** | `Netlist`, `LayoutMetrics` and `SchematicLayout` are plain structs passed between layers. |
| **RAII / clear ownership** | `std::unique_ptr<Simulator>` in `MainWindow`, and Qt parent ownership for widgets and graphics items. |

### Possible Improvements

- **Event-driven simulation**: re-evaluate only the gates whose inputs changed, using an event queue, instead of full passes.
- **Left-edge track assignment**: let nets whose vertical spans don't overlap share a routing track, giving narrower channels.

---

## 📂 Project Structure

```text
MiniEDA/
├── CMakeLists.txt            # Core/layout libraries, app, and per-test CTest registration
├── include/
│   ├── netlist.hpp           # LogicState, GateType, Gate, Netlist (plain data)
│   ├── lexer.hpp             # Token, SyntaxException, tokenize()
│   ├── parser.hpp            # Parser: Verilog text -> Netlist
│   ├── simulator.hpp         # Simulator: tri-state evaluation of a Netlist
│   ├── schematicLayout.hpp   # Placement and wire routing (pure geometry)
│   ├── circuitItems.hpp      # Input/output port and gate items, colour palette
│   ├── schematicScene.hpp    # Builds and refreshes the drawn circuit
│   ├── zoomableView.hpp      # Ctrl+wheel zoom, fit-to-window, pan
│   ├── hoverSplitter.hpp     # Editor/schematic divider highlighted on mouse over
│   └── mainWindow.hpp        # Editor + schematic window
├── src/                      # Implementations of the headers above, plus main.cpp
├── tests/
│   ├── testUtils.hpp         # Shared helpers (load examples, parse)
│   ├── parserTest.cpp        # Parsing, comments, vectors, assign, syntax errors
│   ├── simulatorTest.cpp     # Truth tables, Z handling, mux, shift register
│   ├── layoutTest.cpp        # No overlapping wires, no wire through a gate, every pin wired
│   └── uiTest.cpp            # Real clicks on the schematic, black text, theme toggle
└── verilog/                  # Example designs (gates, dataflow, decoder, mux, shift register, ...)
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

## 🖱️ Using the Simulator

1. Write Verilog in the editor and press **Simulate**, or press **Open File** and pick a `.v` file (for example from `verilog/`).
2. Click an input box to toggle it. Inputs start at `0` and toggle `0 ↔ 1`; the circuit re-evaluates after every click.
3. Read the result from the colours:

| Element | `1` | `0` | `Z` |
|---|---|---|---|
| Wires | red (thick) | blue (thick) | orange (thin) |
| Gates | green | gray | light orange |
| Outputs | green | gray | light orange |

Each gate box also shows its input and output values, e.g. `In: [1, 0, 1]`, `Out: [0]`.

### Zoom & Navigation

| Action | Mouse / Button | Keyboard |
|---|---|---|
| Zoom in / out | Ctrl + mouse wheel, `+` / `-` buttons | Ctrl + `+` / Ctrl + `-` |
| Fit circuit to window | `Fit` button | Ctrl + `0` |
| Actual size | `100%` button | Ctrl + `1` |
| Pan | Drag on empty canvas, or scroll | |
| Dark / light theme | `Light Theme` / `Dark Theme` button (right of the zoom controls) | |
| Resize editor / schematic | Drag the divider between the two panels (highlighted on mouse over) | |

The application starts in the system theme. With Qt 6.8 or newer the theme switch uses Qt's native colour scheme; with older Qt it switches to the Fusion style with a dark or light palette. The schematic keeps its light boxes and black text in both themes for contrast.

Large circuits are fitted to the window automatically when loaded, and stay fitted while the window is resized until you zoom manually.

### Sequential Circuits

Latches and flip-flops start in an unknown (`Z`) state, as in real hardware. Give them a defined state first, then clock them. For `verilog/08_univ_shift.v`:

1. Toggle `RST` to `1` and back to `0` → `Q1Q0 = 00`.
2. Select a mode with `S1 S0` (`00` hold, `01` shift-right, `10` shift-left, `11` parallel load) and set the data inputs (`I0`, `I1`, `RIN`, `LIN`).
3. Toggle `CLK` `0 → 1`: the outputs update on the rising edge. Toggle it back to `0` before the next edge.

---

## 🧪 Running the Unit Test Suite

There is one test executable per component, and each test function is registered as its own CTest entry (`Parser.<testName>`, `Simulator.<testName>`, `Layout.<testName>`, `Ui.<testName>`). CTest runs them with `QT_QPA_PLATFORM=offscreen`, so no display is needed. The names are discovered automatically from the `void testX();` declarations, so new tests appear after a normal build. The suites cover:

- **Parser**: port/wire/gate parsing, vectors, bit selects and `assign` expressions; comments containing keywords (including a commented design compared against its clean version); precise error coordinates and malformed input; every file in `verilog/` must parse, have a module name and contain gates.
- **Simulator**: tri-state truth tables, controlling-value resolution, `assign` and vector evaluation, the 4-to-1 mux, and a full sequence on the universal shift register (reset, load, hold, shift-right, shift-left).
- **Layout**: for every file in `verilog/`, with normal and oversized boxes, wires of different nets never overlap, no wire passes through a gate, every segment is horizontal or vertical and stays inside the module frame; every gate input and output port is wired.
- **Ui**: loading draws every element with inputs at 0; real mouse clicks drive the shift register; schematic text stays black under a dark palette; the theme button toggles dark/light and sits right of the zoom controls.

Testing techniques used:

- **Invariant checks**: the layout tests don't compare exact coordinates; they verify rules that must hold for any design (no overlaps, no wire through a gate, orthogonal segments, every pin wired), so the layout can evolve without rewriting tests.
- **Differential testing**: the same design is parsed with and without comments, and both versions must produce an identical netlist and identical simulation results for every input combination.
- **Real example designs**: every file in `verilog/` is loaded automatically by the parser and layout suites, so a new example is covered as soon as it is added.

Run all tests from the `build` directory:
```bash
ctest --output-on-failure
```

Run one test:
```bash
ctest -R Parser.testAssignExpressions
```

Show the detailed QtTest output of one suite:
```bash
./parserTest      # or ./simulatorTest, ./layoutTest, ./uiTest (with QT_QPA_PLATFORM=offscreen if there is no display)
```

---

## 📑 Supported Verilog Subset

Structural netlists with ANSI-style port declarations:

```verilog
module MixedControlUnit (
    input SystemEnable,
    input SignalA,
    input SignalB,
    output Alert,
    output ReadySignal,
    output SafeStatus
);
    wire netInternal;   // Internal net

    nand check1 (netInternal, SignalA, SignalB);
    nor  check2 (Alert, netInternal, SystemEnable);
    xnor check3 (ReadySignal, SignalA, SystemEnable);
    buf  buffer1 (SafeStatus, SystemEnable);
endmodule
```

Dataflow assignments, vectors and bit selects:

```verilog
module dataflow_logic (
    input [1:0] a,
    input b,
    output y,
    output z
);
    /* Boolean equations are lowered into gates */
    assign y = (a[0] & b) | (~a[1]);
    assign z = (a[0] ^ b) & y;
endmodule
```

| Supported | Not supported (yet) |
|---|---|
| `module` with ANSI or non-ANSI port lists | Multiple modules / hierarchy |
| `input`, `output`, `wire` (scalar and `[msb:lsb]`) | Constants (`1'b0`, `1'b1`) |
| Gates: `and or xor nand nor xnor buf not` (n inputs) | Whole-vector operations (`assign y = a & b` on buses) |
| `assign` with `~ ! & && ^ ~^ ^~ \| \|\|` and `( )` | `?:`, reduction operators (`&a`, `\|a`) |
| Bit selects `a[0]` | `always` blocks, `reg` behaviour |
| `//` and `/* */` comments | |
