cmake_minimum_required(VERSION 3.16)
project(MiniEDA VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Activăm suportul global pentru testare în CMake
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)

# 1. Nucleul fără interfață grafică: lexer, parser și simulator (C++ standard, fără Qt)
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

# 2. Așezarea schemei și rutarea firelor: geometrie pură (doar Qt Core), testabilă fără interfață
add_library(MiniEDALayout STATIC
    src/schematicLayout.cpp
    include/schematicLayout.hpp
)
target_link_libraries(MiniEDALayout PUBLIC MiniEDACore Qt6::Core)
set_target_properties(MiniEDALayout PROPERTIES AUTOMOC OFF)

# 3. Aplicația Principală Grafică
add_executable(MiniEDA
    src/main.cpp
    src/mainWindow.cpp
    src/schematicScene.cpp
    src/circuitItems.cpp
    src/zoomableView.cpp
    include/mainWindow.hpp
    include/schematicScene.hpp
    include/circuitItems.hpp
    include/zoomableView.hpp
)
target_link_libraries(MiniEDA PRIVATE MiniEDALayout Qt6::Widgets)

# 4. Executabilele de Unit Test, câte unul pentru fiecare fișier din tests/.
# Fiecare funcție de test QtTest devine un test separat în CTest (<Prefix>.<testName>);
# numele sunt extrase automat din declarațiile "void testX();", iar CMake se
# reconfigurează singur când fișierul de test se modifică.
function(add_qt_test_suite target prefix)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/tests/${target}.cpp)
    add_executable(${target} ${source} tests/testUtils.hpp)
    target_link_libraries(${target} PRIVATE ${ARGN} Qt6::Test)
    # Testele pot încărca direct exemplele din verilog/
    target_compile_definitions(${target} PRIVATE MINIEDA_VERILOG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/verilog")

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${source})
    file(STRINGS ${source} decls REGEX "^[ \t]*void[ \t]+test[A-Za-z0-9_]*[ \t]*\\([ \t]*\\)[ \t]*;")
    foreach(decl ${decls})
        string(REGEX REPLACE "^[ \t]*void[ \t]+(test[A-Za-z0-9_]*).*" "\\1" testName "${decl}")
        add_test(NAME ${prefix}.${testName} COMMAND ${target} ${testName})
    endforeach()
endfunction()

add_qt_test_suite(parserTest Parser MiniEDACore)
add_qt_test_suite(simulatorTest Simulator MiniEDACore)
add_qt_test_suite(layoutTest Layout MiniEDALayout)
