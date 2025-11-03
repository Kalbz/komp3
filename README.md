# MiniJava Compiler

This project is for compiler course at BTH, where the goal is to create a compiler for the  **MiniJava** language.

## Features
- Lexical analysis using **Flex** (`lexer.flex`)
- Parsing using **Yacc/Bison** (`parser.yy`, `grammar.g`)
- Semantic analysis and symbol-table construction in modern C++ (`SemanticAnalyzer.hpp/.cpp`)
- Compilation pipeline and backends (`main.cc`, `IR*`, `CppCodeGenerator*`, `AssemblyGenerator*`)

