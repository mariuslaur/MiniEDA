cmake_minimum_required(VERSION 3.16)
project(MiniEDA VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Enable CTest support for the whole project
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)

# 1. Core without GUI: lexer, parser and simulator (standard C++, no Qt)
add_library(MiniEDACore STATIC
    src/lexer.cpp
    src/parser.cpp
    src/simulator.cpp
    include/netlist.hpp
    include/lexer.hpp
    include/parser.hpp
    include/simulator.hpp
)
target_include_directories(MiniEDACore PUBLIC include)
set_target_properties(MiniEDACore PROPERTIES AUTOMOC OFF)

# 2. Schematic placement and wire routing: pure geometry (Qt Core only), testable without a GUI
add_library(MiniEDALayout STATIC
    src/schematicLayout.cpp
    include/schematicLayout.hpp
)
target_link_libraries(MiniEDALayout PUBLIC MiniEDACore Qt6::Core)
set_target_properties(MiniEDALayout PROPERTIES AUTOMOC OFF)

# 3. Graphical interface (a library, so it can be tested), plus the application that starts it
add_library(MiniEDAUi STATIC
    src/mainWindow.cpp
    src/schematicScene.cpp
    src/circuitItems.cpp
    src/zoomableView.cpp
    src/hoverSplitter.cpp
    include/hoverSplitter.hpp
    include/mainWindow.hpp
    include/schematicScene.hpp
    include/circuitItems.hpp
    include/zoomableView.hpp
)
target_link_libraries(MiniEDAUi PUBLIC MiniEDALayout Qt6::Widgets)

add_executable(MiniEDA src/main.cpp)
target_link_libraries(MiniEDA PRIVATE MiniEDAUi)

# 4. Unit test executables, one per file in tests/.
# Every QtTest function becomes a separate CTest entry (<Prefix>.<testName>);
# the names are extracted automatically from the "void testX();" declarations, and
# CMake reconfigures itself when the test file changes.
function(add_qt_test_suite target prefix)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/tests/${target}.cpp)
    add_executable(${target} ${source} tests/testUtils.hpp)
    target_link_libraries(${target} PRIVATE ${ARGN} Qt6::Test)
    # Tests can load the example designs from verilog/ directly
    target_compile_definitions(${target} PRIVATE MINIEDA_VERILOG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/verilog")

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${source})
    file(STRINGS ${source} decls REGEX "^[ \t]*void[ \t]+test[A-Za-z0-9_]*[ \t]*\\([ \t]*\\)[ \t]*;")
    foreach(decl ${decls})
        string(REGEX REPLACE "^[ \t]*void[ \t]+(test[A-Za-z0-9_]*).*" "\\1" testName "${decl}")
        add_test(NAME ${prefix}.${testName} COMMAND ${target} ${testName})
        # Tests run without a display (also in CI)
        set_tests_properties(${prefix}.${testName} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endforeach()
endfunction()

add_qt_test_suite(parserTest Parser MiniEDACore)
add_qt_test_suite(simulatorTest Simulator MiniEDACore)
add_qt_test_suite(layoutTest Layout MiniEDALayout)
add_qt_test_suite(uiTest Ui MiniEDAUi)
