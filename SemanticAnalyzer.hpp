#ifndef SEMANTIC_ANALYZER_HPP
#define SEMANTIC_ANALYZER_HPP

#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

#include "Node.h"

struct VariableInfo {
    std::string name;
    std::string type;
    int line = 0;
};

struct MethodInfo {
    std::string name;
    std::string returnType;
    int line = 0;
    bool isStatic = false;
    bool isMain = false;
    std::vector<VariableInfo> parameters;
    std::unordered_map<std::string, VariableInfo> locals;
    Node *body = nullptr;
};

struct ClassInfo {
    std::string name;
    std::string superClass;
    int line = 0;
    std::unordered_map<std::string, VariableInfo> fields;
    std::unordered_map<std::string, MethodInfo> methods;
    Node *declaration = nullptr;
};

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(Node *rootNode);

    bool buildSymbolTable();
    bool performSemanticAnalysis();

    void printSymbolTable(std::ostream &os) const;
    void generateSymbolTableDot(const std::string &filename) const;
    void reportErrors(std::ostream &os) const;
    bool hasErrors() const;

    const std::unordered_map<std::string, ClassInfo> &getClasses() const { return classes; }
    const std::string &getMainClassName() const { return mainClassName; }

private:
    Node *root;
    std::unordered_map<std::string, ClassInfo> classes;
    std::string mainClassName;
    std::vector<std::string> errors;

    // symbol table construction helpers
    void reset();
    void buildGoal(Node *goal);
    void buildMainClass(Node *mainClass);
    void buildClassDeclaration(Node *classDecl);
    void buildClassDeclarationList(Node *list);
    void buildVarDeclarationList(Node *list, std::unordered_map<std::string, VariableInfo> &target, const std::string &scopeName);
    void buildMethodDeclarationList(Node *list, ClassInfo &cls);
    void buildMethodDeclaration(Node *methodNode, ClassInfo &cls);
    void buildMainMethod(Node *methodNode, ClassInfo &cls);
    void collectMethodLocals(Node *bodyNode, MethodInfo &method, const std::string &scopeName);
    void collectStatementsForLocals(Node *node, MethodInfo &method, const std::string &scopeName);
    void validateTypeReferences();

    // semantic analysis helpers
    void analyzeClass(const ClassInfo &cls);
    void analyzeMethod(const ClassInfo &cls, const MethodInfo &method);
    void analyzeBody(Node *body, const ClassInfo &cls, const MethodInfo &method);
    void analyzeMainBody(Node *body, const ClassInfo &cls, const MethodInfo &method);
    void analyzeStatement(Node *stmt, const ClassInfo &cls, const MethodInfo &method);
    void analyzeStatementList(Node *node, const ClassInfo &cls, const MethodInfo &method);
    void analyzeConditionalBranch(Node *branch, const ClassInfo &cls, const MethodInfo &method);
    std::string analyzeExpression(Node *expr, const ClassInfo &cls, const MethodInfo &method);
    std::string analyzeMethodInvocation(Node *invocation, const ClassInfo &cls, const MethodInfo &method);
    std::string analyzeIndexExpression(Node *indexNode, const ClassInfo &cls, const MethodInfo &method, bool isAssignmentLhs);
    std::vector<std::string> analyzeArguments(Node *argsNode, const ClassInfo &cls, const MethodInfo &method);

    // lookup helpers
    std::string lookupVariable(const ClassInfo &cls, const MethodInfo &method, const std::string &name, int line);
    const MethodInfo *lookupMethod(const std::string &className, const std::string &methodName) const;
    const ClassInfo *lookupClass(const std::string &className) const;

    // utilities
    static std::string makeTypeFromNode(Node *typeNode);
    static int nodeLine(Node *node);
    static bool isPrimitive(const std::string &typeName);
    static bool isArrayType(const std::string &typeName);
    static bool typesMatch(const std::string &lhs, const std::string &rhs);
    void addError(int line, const std::string &message);
};

#endif // SEMANTIC_ANALYZER_HPP
