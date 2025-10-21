all: compiler

c: clean all

OBJ = parser.tab.o SemanticAnalyzer.o IR.o IRGenerator.o CppCodeGenerator.o AssemblyGenerator.o main.o

compiler: lex.yy.c $(OBJ)
	g++ -g -O0 -w -o compiler $(OBJ) lex.yy.c -std=c++14

parser.tab.o: parser.tab.cc
	g++ -g -w -c parser.tab.cc -std=c++14

SemanticAnalyzer.o: SemanticAnalyzer.cpp SemanticAnalyzer.hpp
	g++ -g -w -c SemanticAnalyzer.cpp -std=c++14

IR.o: IR.cpp IR.hpp
	g++ -g -w -c IR.cpp -std=c++14

IRGenerator.o: IRGenerator.cpp IRGenerator.hpp IR.hpp SemanticAnalyzer.hpp
	g++ -g -w -c IRGenerator.cpp -std=c++14

CppCodeGenerator.o: CppCodeGenerator.cpp CppCodeGenerator.hpp IR.hpp SemanticAnalyzer.hpp
	g++ -g -w -c CppCodeGenerator.cpp -std=c++14

AssemblyGenerator.o: AssemblyGenerator.cpp AssemblyGenerator.hpp IR.hpp
	g++ -g -w -c AssemblyGenerator.cpp -std=c++14

main.o: main.cc parser.tab.hh IRGenerator.hpp IR.hpp SemanticAnalyzer.hpp
	g++ -g -w -c main.cc -std=c++14

parser.tab.cc: parser.yy
	bison parser.yy

lex.yy.c: lexer.flex parser.tab.cc
	flex lexer.flex

tree:
	dot -Tpdf tree.dot -o tree.pdf

st:
	dot -Tpdf symbol_table.dot -o symbol_table.pdf

cfg:
	dot -Tpdf cfg.dot -o cfg.pdf

clean:
	rm -f parser.tab.* lex.yy.c* compiler stack.hh position.hh location.hh tree.dot tree.pdf
	rm -f *.o cfg.dot cfg.pdf
	rm -Rf compiler.dSYM
