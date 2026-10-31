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

# Înregistrăm fiecare funcție de test QtTest ca test separat în CTest.
# Numele sunt extrase automat din declarațiile "void testX();" din parserTest.cpp,
# iar CMake se reconfigurează singur când fișierul de test se modifică.
set(PARSER_TEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/tests/parserTest.cpp)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PARSER_TEST_SOURCE})
file(STRINGS ${PARSER_TEST_SOURCE} PARSER_TEST_DECLS REGEX "^[ \t]*void[ \t]+test[A-Za-z0-9_]*[ \t]*\\([ \t]*\\)[ \t]*;")
foreach(decl ${PARSER_TEST_DECLS})
    string(REGEX REPLACE "^[ \t]*void[ \t]+(test[A-Za-z0-9_]*).*" "\\1" testName "${decl}")
    add_test(NAME Parser.${testName} COMMAND parserTest ${testName})
endforeach()