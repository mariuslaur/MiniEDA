cmake_minimum_required(VERSION 3.16)
project(MiniEDA VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Activăm suportul global pentru testare în CMake
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)

# 1. Aplicația Principală Grafică
file(GLOB_RECURSE SOURCES "src/*.cpp" "include/*.hpp")
add_executable(MiniEDA ${SOURCES})
target_include_directories(MiniEDA PRIVATE include)
target_link_libraries(MiniEDA PRIVATE Qt6::Widgets)

# 2. Executabilul de Unit Test (Aparține exclusiv procesului de testare)
add_executable(parserTest 
    tests/parserTest.cpp
    src/parser.cpp       # Includem unitatea pe care o testăm direct
    include/parser.hpp
)
target_include_directories(parserTest PRIVATE include)
target_link_libraries(parserTest PRIVATE Qt6::Test Qt6::Widgets)

# Înregistrăm testul în utilitarul CTest al CMake
add_test(NAME MiniEDAParserTest COMMAND parserTest)