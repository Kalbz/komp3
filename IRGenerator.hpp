#ifndef IR_GENERATOR_HPP
#define IR_GENERATOR_HPP

#include <string>
#include <vector>

#include "IR.hpp"
#include "SemanticAnalyzer.hpp"

class IRGenerator {
public:
    explicit IRGenerator(const SemanticAnalyzer &analyzer);

    const ir::Module &generate();
    const ir::Module &getModule() const { return module; }
    bool hasErrors() const { return !errors.empty(); }
    const std::vector<std::string> &getErrors() const { return errors; }

private:
    const SemanticAnalyzer &semanticAnalyzer;
    ir::Module module;
    std::vector<std::string> errors;

    int tempCounter = 0;
    int blockCounter = 0;

    ir::Function *currentFunction = nullptr;
    ir::BasicBlock *currentBlock = nullptr;

    std::string nextTemp(const std::string &hint = "t");
    std::string nextBlockName(const std::string &hint);

    void addError(const Node *node, const std::string &message);
    void addError(int line, const std::string &message);
    void addError(const std::string &message);

    void generateForClass(const ClassInfo &cls);
    void generateForMethod(const ClassInfo &cls, const MethodInfo &method);
    void generateBody(Node *bodyNode);
    void generateStatement(Node *statementNode);
    void generateStatementList(Node *listNode);
    void generateIf(Node *node);
    void generateWhile(Node *node);
    void generatePrint(Node *node);
    void generateAssignment(Node *node);
    void generateReturn(Node *node);

    std::string generateExpression(Node *expr);
    std::string generateBinary(const std::string &opName, ir::Opcode opCode, Node *node);
    std::string generateUnary(ir::Opcode opCode, Node *node);
    std::string generateIdentifier(Node *node);
    std::string generateLiteral(Node *node);
    std::string generateMethodInvocation(Node *node);
    std::string generateIndex(Node *node);

    void ensureCurrentBlock();
};

#endif // IR_GENERATOR_HPP
