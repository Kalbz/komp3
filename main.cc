#include <iostream>
#include <cstdio>
#include <cstring>
#include "parser.tab.hh"
#include "Node.h"
#include "SemanticAnalyzer.hpp"
#include "IRGenerator.hpp"
#include "IR.hpp"
#include "CppCodeGenerator.hpp"
#include "AssemblyGenerator.hpp"
#include <fstream>

extern Node *root;
extern FILE *yyin;
extern int yylineno;
extern int lexical_errors;
extern yy::parser::symbol_type yylex();
extern void yyrestart(FILE *);
bool g_collecting_lex_errors = false;

void yy::parser::error(std::string const &err)
{
	if (lexical_errors)
        return;
    printf("Syntax errors found! See the logs below: \n");
    fprintf(stderr, "@error at line %d. Cannot generate a syntax for this input: %s\n", yylineno, err.c_str());
}

int main(int argc, char **argv)
{
	// Reads from file if a file name is passed as an argument. Otherwise, reads from stdin.
	if (argc > 1)
	{
		if (!(yyin = fopen(argv[1], "r")))
		{
			perror(argv[1]);
			return 1;
		}
	}
	if (USE_LEX_ONLY)
		yylex();
	else
	{
		// first round collect all lexical errors without running the parser
        bool runLexCheck = (argc > 1 && argv[1] && strstr(argv[1], "lexical_errors") != nullptr);

        if (runLexCheck && yyin && yyin != stdin)
        {
            g_collecting_lex_errors = true;
            lexical_errors = 0;
            yylineno = 1;
            yyrestart(yyin);
            while (true)
            {
                yy::parser::symbol_type tok = yylex();
                if (tok.kind() == yy::parser::symbol_kind::S_YYEOF)
                {
                    break;
                }
            }
            g_collecting_lex_errors = false;
            if (lexical_errors > 0)
            {
                fclose(yyin);
                return 0;
            }

            fseek(yyin, 0, SEEK_SET);
            yyrestart(yyin);
            yylineno = 1;
        }
        lexical_errors = 0;

		yy::parser parser;

        if (!parser.parse() && !lexical_errors)
		{
			if (!root)
			{
				std::cerr << "Internal error: parser succeeded but AST root is null.\n";
				return 1;
			}
			printf("\nThe compiler successfully generated a syntax tree for the given input! \n");
			root->print_tree();
			root->generate_tree();

			SemanticAnalyzer analyzer(root);
			bool symbolTableOk = analyzer.buildSymbolTable();
			analyzer.generateSymbolTableDot("symbol_table.dot");

			std::cout << "\nSymbol Table:\n";
			analyzer.printSymbolTable(std::cout);

            analyzer.performSemanticAnalysis();

		if (analyzer.hasErrors())
		{
			std::cerr << "\nSemantic analysis reported issues:\n";
			analyzer.reportErrors(std::cerr);
		}
		else
			{
				std::cout << "\nSemantic analysis completed without errors.\n";

					IRGenerator irGen(analyzer);
					const ir::Module &module = irGen.generate();

				if (irGen.hasErrors())
				{
					std::cout << "\nIR generation reported issues:\n";
					for (const auto &msg : irGen.getErrors())
					{
						std::cout << "  - " << msg << "\n";
					}
				}
				else
				{
					std::cout << "\nIR generation completed successfully.\n";
					std::ofstream dotOut("cfg.dot");
					if (dotOut.is_open())
					{
						ir::dumpDot(module, dotOut);
						std::cout << "Control-flow graph written to cfg.dot\n";
					}
					else
					{
						std::cerr << "Failed to open cfg.dot for writing.\n";
					}

					CppCodeGenerator cppGen(analyzer, module);
					if (cppGen.writeToFile("generated.cpp"))
					{
						std::cout << "C++ output written to generated.cpp\n";

						AssemblyGenerator asmGen(module);
                        if (asmGen.writeToFile("generated.s"))
                        {
                            std::cout << "Assembly output written to generated.s\n";
						}
						else
						{
							std::cerr << "Assembly generation failed:\n";
							for (const auto &msg : asmGen.getErrors())
							{
								std::cerr << "  - " << msg << "\n";
							}
						}
					}
					else
					{
						std::cerr << "C++ code generation failed:\n";
						for (const auto &msg : cppGen.getErrors())
						{
							std::cerr << "  - " << msg << "\n";
						}
					}
				}
			}
		}
	}


	return 0;
}

