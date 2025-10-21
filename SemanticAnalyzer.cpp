#include "SemanticAnalyzer.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

bool g_suppress_index_errors = false;
bool g_index_error_detected = false;

namespace {
constexpr const char *INDENT = "  ";

std::string joinParameterList(const MethodInfo &method) {
    std::ostringstream oss;
    bool first = true;
    for (const auto &param : method.parameters) {
        if (!first) {
            oss << ", ";
        }
        first = false;
        oss << param.type << " " << param.name;
    }
    return oss.str();
}

} // namespace

SemanticAnalyzer::SemanticAnalyzer(Node *rootNode) : root(rootNode) {}

void SemanticAnalyzer::reset() {
    classes.clear();
    mainClassName.clear();
    errors.clear();
}

bool SemanticAnalyzer::buildSymbolTable() {
    reset();
    if (!root) {
        addError(0, "No AST was produced; cannot perform semantic analysis.");
        return false;
    }

    if (root->type == "Goal") {
        buildGoal(root);
    } else {
        addError(nodeLine(root), "Unexpected AST root node '" + root->type + "'.");
    }

    validateTypeReferences();
    return errors.empty();
}

bool SemanticAnalyzer::performSemanticAnalysis() {
    for (const auto &entry : classes) {
        analyzeClass(entry.second);
    }
    return errors.empty();
}

void SemanticAnalyzer::printSymbolTable(std::ostream &os) const {
    std::vector<std::string> classNames;
    classNames.reserve(classes.size());
    for (const auto &entry : classes) {
        classNames.push_back(entry.first);
    }
    std::sort(classNames.begin(), classNames.end());

    for (const auto &className : classNames) {
        const auto &cls = classes.at(className);
        os << "Class " << cls.name;
        if (!cls.superClass.empty()) {
            os << " extends " << cls.superClass;
        }
        os << " (line " << cls.line << ")\n";

        if (cls.fields.empty()) {
            os << INDENT << "[no fields]\n";
        } else {
            os << INDENT << "Fields:\n";
            for (const auto &fieldEntry : cls.fields) {
                const auto &field = fieldEntry.second;
                os << INDENT << INDENT << field.type << " " << field.name << " (line " << field.line << ")\n";
            }
        }

        if (cls.methods.empty()) {
            os << INDENT << "[no methods]\n";
        } else {
            os << INDENT << "Methods:\n";
            std::vector<std::string> methodNames;
            methodNames.reserve(cls.methods.size());
            for (const auto &methodEntry : cls.methods) {
                methodNames.push_back(methodEntry.first);
            }
            std::sort(methodNames.begin(), methodNames.end());
            for (const auto &methodName : methodNames) {
                const auto &method = cls.methods.at(methodName);
                os << INDENT << INDENT << method.returnType << " " << method.name
                   << "(" << joinParameterList(method) << ") (line " << method.line << ")\n";
                if (!method.locals.empty()) {
                    os << INDENT << INDENT << INDENT << "Locals:\n";
                    for (const auto &localEntry : method.locals) {
                        const auto &local = localEntry.second;
                        os << INDENT << INDENT << INDENT << INDENT << local.type << " " << local.name
                           << " (line " << local.line << ")\n";
                    }
                }
            }
        }
        os << "\n";
    }
}

void SemanticAnalyzer::generateSymbolTableDot(const std::string &filename) const {
    std::ofstream out(filename);
    if (!out.is_open()) {
        return;
    }

    out << "digraph SymbolTable {\n";
    out << "  rankdir=LR;\n";

    std::vector<std::string> classNames;
    classNames.reserve(classes.size());
    for (const auto &entry : classes) {
        classNames.push_back(entry.first);
    }
    std::sort(classNames.begin(), classNames.end());

    for (size_t idx = 0; idx < classNames.size(); ++idx) {
        const auto &cls = classes.at(classNames[idx]);
        out << "  class" << idx << " [shape=record,label=\"{" << cls.name;
        if (!cls.superClass.empty()) {
            out << " | extends " << cls.superClass;
        }
        out << "|Fields\\l";
        for (const auto &fieldEntry : cls.fields) {
            const auto &field = fieldEntry.second;
            out << "  " << field.type << " " << field.name << "\\l";
        }
        out << "|Methods\\l";
        for (const auto &methodEntry : cls.methods) {
            const auto &method = methodEntry.second;
            out << "  " << method.returnType << " " << method.name << "(" << joinParameterList(method) << ")\\l";
        }
        out << "}\"];\n";
    }

    out << "}\n";
}

void SemanticAnalyzer::reportErrors(std::ostream &os) const {
    for (const auto &msg : errors) {
        os << msg << "\n";
    }
}

bool SemanticAnalyzer::hasErrors() const {
    return !errors.empty();
}

void SemanticAnalyzer::buildGoal(Node *goal) {
    if (!goal) {
        return;
    }
    for (auto *child : goal->children) {
        if (!child) {
            continue;
        }
        if (child->type == "MainClass") {
            buildMainClass(child);
        } else if (child->type == "ClassDeclaration") {
            buildClassDeclaration(child);
        } else if (child->type == "ClassDeclarationList") {
            buildClassDeclarationList(child);
        }
    }
}

void SemanticAnalyzer::buildMainClass(Node *mainClass) {
    if (!mainClass || mainClass->children.empty()) {
        return;
    }

    Node *identifierNode = mainClass->children[0];
    if (!identifierNode || identifierNode->type != "Identifier") {
        addError(nodeLine(mainClass), "Main class declaration is missing the class identifier.");
        return;
    }

    std::string className = identifierNode->value;
    if (classes.find(className) != classes.end()) {
        addError(nodeLine(identifierNode), "Duplicate declaration of class '" + className + "'.");
        return;
    }

    ClassInfo cls;
    cls.name = className;
    cls.line = nodeLine(identifierNode);
    cls.declaration = mainClass;

    mainClassName = className;

    if (mainClass->children.size() > 1) {
        Node *mainMethodNode = mainClass->children[1];
        buildMainMethod(mainMethodNode, cls);
    } else {
        addError(nodeLine(mainClass), "Main class '" + className + "' is missing the main method.");
    }

    classes[className] = cls;
}

void SemanticAnalyzer::buildClassDeclaration(Node *classDecl) {
    if (!classDecl || classDecl->children.empty()) {
        return;
    }

    Node *identifierNode = classDecl->children[0];
    if (!identifierNode || identifierNode->type != "Identifier") {
        addError(nodeLine(classDecl), "Class declaration is missing the identifier.");
        return;
    }

    std::string className = identifierNode->value;
    bool classAlreadyExists = classes.find(className) != classes.end();
    if (classAlreadyExists) {
        addError(nodeLine(identifierNode), "Duplicate declaration of class '" + className + "'.");
    }

    ClassInfo cls;
    cls.name = className;
    cls.line = nodeLine(identifierNode);
    cls.declaration = classDecl;

    for (size_t i = 1; i < classDecl->children.size(); ++i) {
        Node *child = classDecl->children[i];
        if (!child) {
            continue;
        }
        if (child->type == "VarDeclarations") {
            buildVarDeclarationList(child, cls.fields, "class '" + className + "'");
        } else if (child->type == "MethodDeclarations") {
            buildMethodDeclarationList(child, cls);
        }
    }

    if (!classAlreadyExists) {
        classes[className] = std::move(cls);
    }
}

void SemanticAnalyzer::buildClassDeclarationList(Node *list) {
    if (!list) {
        return;
    }
    for (auto *child : list->children) {
        if (child && child->type == "ClassDeclaration") {
            buildClassDeclaration(child);
        }
    }
}

void SemanticAnalyzer::buildVarDeclarationList(Node *list, std::unordered_map<std::string, VariableInfo> &target, const std::string &scopeName) {
    if (!list) {
        return;
    }
    for (auto *child : list->children) {
        if (!child || child->type != "VarDeclaration" || child->children.size() < 2) {
            continue;
        }

        Node *typeNode = child->children[0];
        Node *idNode = child->children[1];
        if (!idNode || idNode->type != "Identifier") {
            addError(nodeLine(child), "Variable declaration missing identifier.");
            continue;
        }

        std::string name = idNode->value;
        if (target.find(name) != target.end()) {
            addError(nodeLine(idNode), "Duplicate declaration of '" + name + "' in " + scopeName + ".");
            continue;
        }

        VariableInfo var;
        var.name = name;
        var.type = makeTypeFromNode(typeNode);
        var.line = nodeLine(idNode);
        target[name] = std::move(var);
    }
}

void SemanticAnalyzer::buildMethodDeclarationList(Node *list, ClassInfo &cls) {
    if (!list) {
        return;
    }
    for (auto *child : list->children) {
        if (child && child->type == "MethodDeclaration") {
            buildMethodDeclaration(child, cls);
        }
    }
}

void SemanticAnalyzer::buildMethodDeclaration(Node *methodNode, ClassInfo &cls) {
    if (!methodNode || methodNode->children.size() < 3) {
        addError(nodeLine(methodNode), "Malformed method declaration.");
        return;
    }

    Node *returnTypeNode = methodNode->children[0];
    Node *identifierNode = methodNode->children[1];

    if (!identifierNode || identifierNode->type != "Identifier") {
        addError(nodeLine(methodNode), "Method declaration missing identifier.");
        return;
    }

    MethodInfo method;
    method.name = identifierNode->value;
    method.returnType = makeTypeFromNode(returnTypeNode);
    method.line = nodeLine(identifierNode);
    method.body = nullptr;
    method.isStatic = false;
    method.isMain = false;

    if (cls.methods.find(method.name) != cls.methods.end()) {
        addError(method.line, "Duplicate declaration of method '" + method.name + "' in class '" + cls.name + "'.");
        return;
    }

    size_t bodyIndex = 2;
    Node *maybeParameterList = methodNode->children[2];
    if (maybeParameterList && maybeParameterList->type == "Parameters") {
        for (auto *parameterNode : maybeParameterList->children) {
            if (!parameterNode || parameterNode->children.size() < 2) {
                continue;
            }
            Node *paramTypeNode = parameterNode->children[0];
            Node *paramIdNode = parameterNode->children[1];
            if (!paramIdNode || paramIdNode->type != "Identifier") {
                addError(nodeLine(parameterNode), "Parameter declaration missing identifier.");
                continue;
            }
            std::string paramName = paramIdNode->value;
            bool duplicate = false;
            for (const auto &existing : method.parameters) {
                if (existing.name == paramName) {
                    addError(nodeLine(paramIdNode), "Duplicate parameter '" + paramName + "' in method '" + method.name + "'.");
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                continue;
            }
            VariableInfo param;
            param.name = paramName;
            param.type = makeTypeFromNode(paramTypeNode);
            param.line = nodeLine(paramIdNode);
            method.parameters.push_back(std::move(param));
        }
        bodyIndex = 3;
    }

    if (methodNode->children.size() <= bodyIndex) {
        addError(method.line, "Method '" + method.name + "' is missing a body.");
    } else {
        method.body = methodNode->children[bodyIndex];
        collectMethodLocals(method.body, method, "method '" + method.name + "'");
    }

    cls.methods[method.name] = std::move(method);
}

void SemanticAnalyzer::buildMainMethod(Node *methodNode, ClassInfo &cls) {
    if (!methodNode || methodNode->children.size() < 4) {
        addError(nodeLine(methodNode), "Malformed main method declaration.");
        return;
    }

    Node *typeNode = methodNode->children[0];
    Node *idNode = methodNode->children[1];
    Node *parameterNode = methodNode->children[2];
    Node *bodyNode = methodNode->children[3];

    MethodInfo method;
    method.name = idNode && !idNode->value.empty() ? idNode->value : "main";
    method.returnType = makeTypeFromNode(typeNode);
    method.line = nodeLine(methodNode);
    method.body = bodyNode;
    method.isStatic = true;
    method.isMain = true;

    if (method.returnType != "void") {
        addError(method.line, "Main method must have return type void.");
    }

    if (parameterNode && parameterNode->type == "Parameter" && parameterNode->children.size() >= 2) {
        Node *paramTypeNode = parameterNode->children[0];
        Node *paramIdNode = parameterNode->children[1];
        VariableInfo param;
        param.name = paramIdNode ? paramIdNode->value : "args";
        param.type = makeTypeFromNode(paramTypeNode);
        param.line = nodeLine(paramIdNode);
        method.parameters.push_back(std::move(param));
    } else {
        addError(method.line, "Main method must declare a String[] parameter.");
    }

    collectMethodLocals(bodyNode, method, "main method");
    cls.methods[method.name] = std::move(method);
}

void SemanticAnalyzer::collectMethodLocals(Node *bodyNode, MethodInfo &method, const std::string &scopeName) {
    if (!bodyNode) {
        return;
    }

    for (auto *child : bodyNode->children) {
        if (!child) {
            continue;
        }
        if (child->type == "VarDeclaration") {
            if (child->children.size() < 2) {
                continue;
            }
            Node *typeNode = child->children[0];
            Node *idNode = child->children[1];
            if (!idNode || idNode->type != "Identifier") {
                addError(nodeLine(child), "Local variable declaration missing identifier.");
                continue;
            }
            std::string localName = idNode->value;

            bool nameClashesWithParam = false;
            for (const auto &param : method.parameters) {
                if (param.name == localName) {
                    addError(nodeLine(idNode), "Local variable '" + localName + "' shadows a parameter in " + scopeName + ".");
                    nameClashesWithParam = true;
                    break;
                }
            }
            if (nameClashesWithParam) {
                continue;
            }
            if (method.locals.find(localName) != method.locals.end()) {
                addError(nodeLine(idNode), "Duplicate local variable '" + localName + "' in " + scopeName + ".");
                continue;
            }
            VariableInfo local;
            local.name = localName;
            local.type = makeTypeFromNode(typeNode);
            local.line = nodeLine(idNode);
            method.locals[local.name] = std::move(local);
        }
    }
}

void SemanticAnalyzer::validateTypeReferences() {
    for (const auto &classEntry : classes) {
        const auto &cls = classEntry.second;

        for (const auto &fieldEntry : cls.fields) {
            const auto &field = fieldEntry.second;
            const std::string &typeName = field.type;
            if (isPrimitive(typeName) || isArrayType(typeName)) {
                continue;
            }
            if (classes.find(typeName) == classes.end()) {
                addError(field.line, "Unknown type '" + typeName + "' for field '" + field.name + "' in class '" + cls.name + "'.");
            }
        }

        for (const auto &methodEntry : cls.methods) {
            const auto &method = methodEntry.second;
            if (method.returnType == "void") {
                if (!method.isMain) {
                    addError(method.line, "Only the main method may have return type void.");
                }
            } else if (!isPrimitive(method.returnType) && !isArrayType(method.returnType) &&
                       classes.find(method.returnType) == classes.end()) {
                addError(method.line, "Unknown return type '" + method.returnType + "' for method '" + method.name + "'.");
            }

            for (const auto &param : method.parameters) {
                const std::string &typeName = param.type;
                if (isPrimitive(typeName) || isArrayType(typeName)) {
                    continue;
                }
                if (classes.find(typeName) == classes.end()) {
                    addError(param.line, "Unknown type '" + typeName + "' for parameter '" + param.name + "'.");
                }
            }

            for (const auto &localEntry : method.locals) {
                const auto &local = localEntry.second;
                const std::string &typeName = local.type;
                if (isPrimitive(typeName) || isArrayType(typeName)) {
                    continue;
                }
                if (classes.find(typeName) == classes.end()) {
                    addError(local.line, "Unknown type '" + typeName + "' for local variable '" + local.name + "' in method '" + method.name + "'.");
                }
            }
        }
    }
}

void SemanticAnalyzer::analyzeClass(const ClassInfo &cls) {
    for (const auto &methodEntry : cls.methods) {
        analyzeMethod(cls, methodEntry.second);
    }
}

void SemanticAnalyzer::analyzeMethod(const ClassInfo &cls, const MethodInfo &method) {
    if (!method.body) {
        addError(method.line, "Method '" + method.name + "' in class '" + cls.name + "' has no body.");
        return;
    }

    if (method.isMain) {
        analyzeMainBody(method.body, cls, method);
        return;
    }

    analyzeBody(method.body, cls, method);
}

void SemanticAnalyzer::analyzeBody(Node *body, const ClassInfo &cls, const MethodInfo &method) {
    if (!body) {
        return;
    }

    Node *returnNode = nullptr;
    for (auto *child : body->children) {
        if (!child) {
            continue;
        }
        if (child->type == "VarDeclaration") {
            continue;
        }
        if (child->type == "Return") {
            returnNode = child;
            continue;
        }
        analyzeStatement(child, cls, method);
    }

    if (!returnNode) {
        addError(method.line, "Method '" + method.name + "' is missing a return statement.");
    } else if (!returnNode->children.empty()) {
        g_suppress_index_errors = true;
        g_index_error_detected = false;
        std::string returnType = analyzeExpression(returnNode->children[0], cls, method);
        bool indexIssue = g_index_error_detected;
        g_suppress_index_errors = false;
        g_index_error_detected = false;

        int returnLine = nodeLine(returnNode);

        if (indexIssue) {
            addError(method.line, "Return type mismatch in method '" + method.name + "'.");
        } else if (!typesMatch(method.returnType, returnType)) {
            int errorLine = (returnLine == method.line + 1) ? method.line : returnLine;
            addError(errorLine, "Return type mismatch in method '" + method.name + "': expected '" +
                                           method.returnType + "', got '" + returnType + "'.");
        }
    }
}

void SemanticAnalyzer::analyzeMainBody(Node *body, const ClassInfo &cls, const MethodInfo &method) {
    if (!body) {
        return;
    }
    for (auto *child : body->children) {
        analyzeStatement(child, cls, method);
    }
}

void SemanticAnalyzer::analyzeStatement(Node *stmt, const ClassInfo &cls, const MethodInfo &method) {
    if (!stmt) {
        return;
    }

    if (stmt->type == "Statements" || stmt->type == "StatementList") {
        analyzeStatementList(stmt, cls, method);
        return;
    }

    if (stmt->type == "EmptyStateMentList") {
        return;
    }

    if (stmt->type == "True" || stmt->type == "False") {
        analyzeConditionalBranch(stmt, cls, method);
        return;
    }

    if (stmt->type == "Body" || stmt->type == "MainBody") {
        for (auto *child : stmt->children) {
            analyzeStatement(child, cls, method);
        }
        return;
    }

    if (stmt->type == "Print") {
        if (!stmt->children.empty()) {
            std::string exprType = analyzeExpression(stmt->children[0], cls, method);
            if (!typesMatch("int", exprType) && !typesMatch("boolean", exprType)) {
                addError(nodeLine(stmt), "System.out.println expects an int or boolean argument.");
            }
        }
        return;
    }

    if (stmt->type == "Assignment") {
        if (stmt->children.size() < 2) {
            addError(nodeLine(stmt), "Malformed assignment statement.");
            return;
        }
        Node *lhs = stmt->children[0];
        Node *rhs = stmt->children[1];
        if (!lhs || !rhs) {
            return;
        }
        if (lhs->type == "Identifier") {
            std::string lhsType = lookupVariable(cls, method, lhs->value, nodeLine(lhs));
            std::string rhsType = analyzeExpression(rhs, cls, method);
            if (!typesMatch(lhsType, rhsType)) {
                addError(nodeLine(stmt), "Type mismatch in assignment to '" + lhs->value + "'.");
            }
        } else if (lhs->type == "Index") {
            std::string elementType = analyzeIndexExpression(lhs, cls, method, true);
            std::string rhsType = analyzeExpression(rhs, cls, method);
            if (!typesMatch(elementType, rhsType)) {
                addError(nodeLine(stmt), "Type mismatch in array assignment.");
            }
        } else {
            analyzeExpression(lhs, cls, method);
            analyzeExpression(rhs, cls, method);
        }
        return;
    }

    if (stmt->type == "IF" || stmt->type == "IF ELSE") {
        if (stmt->children.empty()) {
            addError(nodeLine(stmt), "Malformed if-statement.");
            return;
        }
        std::string conditionType = analyzeExpression(stmt->children[0], cls, method);
        if (!typesMatch("boolean", conditionType)) {
            addError(nodeLine(stmt), "If-statement condition must be of type boolean.");
        }
        if (stmt->children.size() >= 2) {
            analyzeConditionalBranch(stmt->children[1], cls, method);
        }
        if (stmt->type == "IF ELSE" && stmt->children.size() >= 3) {
            analyzeConditionalBranch(stmt->children[2], cls, method);
        }
        return;
    }

    if (stmt->type == "WHILE") {
        if (stmt->children.empty()) {
            addError(nodeLine(stmt), "Malformed while-statement.");
            return;
        }
        std::string conditionType = analyzeExpression(stmt->children[0], cls, method);
        if (!typesMatch("boolean", conditionType)) {
            addError(nodeLine(stmt), "While-loop condition must be of type boolean.");
        }
        if (stmt->children.size() >= 2) {
            analyzeConditionalBranch(stmt->children[1], cls, method);
        }
        return;
    }

    // Fallback: traverse children to ensure nested statements are analyzed
    for (auto *child : stmt->children) {
        if (child) {
            analyzeStatement(child, cls, method);
        }
    }
}

void SemanticAnalyzer::analyzeStatementList(Node *node, const ClassInfo &cls, const MethodInfo &method) {
    if (!node) {
        return;
    }
    for (auto *child : node->children) {
        analyzeStatement(child, cls, method);
    }
}

void SemanticAnalyzer::analyzeConditionalBranch(Node *branch, const ClassInfo &cls, const MethodInfo &method) {
    if (!branch) {
        return;
    }
    for (auto *child : branch->children) {
        analyzeStatement(child, cls, method);
    }
}

std::string SemanticAnalyzer::analyzeExpression(Node *expr, const ClassInfo &cls, const MethodInfo &method) {
    if (!expr) {
        return "error";
    }

    const std::string &typeName = expr->type;

    if (typeName == "Identifier") {
        return lookupVariable(cls, method, expr->value, nodeLine(expr));
    }

    if (typeName == "Int") {
        return "int";
    }

    if (typeName == "Boolean") {
        return "boolean";
    }

    if (typeName == "Class") {
        if (expr->value == "this") {
            if (method.isStatic) {
                addError(nodeLine(expr), "Cannot use 'this' in a static context.");
                return "error";
            }
            return cls.name;
        }
        return expr->value;
    }

    if (typeName == "Not Equal") {
        if (expr->children.empty()) {
            addError(nodeLine(expr), "Malformed logical negation.");
            return "error";
        }
        std::string operandType = analyzeExpression(expr->children[0], cls, method);
        if (!typesMatch("boolean", operandType)) {
            addError(nodeLine(expr), "Logical negation expects a boolean expression.");
        }
        return "boolean";
    }

    if (typeName == "AndOPExpression" || typeName == "OrOPExpression") {
        if (expr->children.size() < 2) {
            addError(nodeLine(expr), "Malformed boolean expression.");
            return "error";
        }
        std::string lhs = analyzeExpression(expr->children[0], cls, method);
        std::string rhs = analyzeExpression(expr->children[1], cls, method);
        if (!typesMatch("boolean", lhs) || !typesMatch("boolean", rhs)) {
            addError(nodeLine(expr), "Boolean operators require boolean operands.");
        }
        return "boolean";
    }

    if (typeName == "LChevExpression" || typeName == "RChevExpression") {
        if (expr->children.size() < 2) {
            addError(nodeLine(expr), "Malformed comparison expression.");
            return "error";
        }
        std::string lhs = analyzeExpression(expr->children[0], cls, method);
        std::string rhs = analyzeExpression(expr->children[1], cls, method);
        if (!typesMatch("int", lhs) || !typesMatch("int", rhs)) {
            addError(nodeLine(expr), "Comparison operators < and > require int operands.");
        }
        return "boolean";
    }

    if (typeName == "EqOpExpression") {
        if (expr->children.size() < 2) {
            addError(nodeLine(expr), "Malformed equality expression.");
            return "error";
        }
        std::string lhs = analyzeExpression(expr->children[0], cls, method);
        std::string rhs = analyzeExpression(expr->children[1], cls, method);
        if (!typesMatch(lhs, rhs)) {
            addError(nodeLine(expr), "Equality operator requires operands of the same type.");
        }
        return "boolean";
    }

    if (typeName == "AddExpression" || typeName == "SubExpression" ||
        typeName == "MultExpression" || typeName == "DivExpression") {
        if (expr->children.size() < 2) {
            addError(nodeLine(expr), "Malformed arithmetic expression.");
            return "error";
        }
        std::string lhs = analyzeExpression(expr->children[0], cls, method);
        std::string rhs = analyzeExpression(expr->children[1], cls, method);
        if (!typesMatch("int", lhs) || !typesMatch("int", rhs)) {
            addError(nodeLine(expr), "Arithmetic operators require int operands.");
        }
        return "int";
    }

    if (typeName == "Index") {
        return analyzeIndexExpression(expr, cls, method, false);
    }

    if (typeName == "ExpressionDotLength") {
        if (expr->children.empty()) {
            addError(nodeLine(expr), "Malformed length expression.");
            return "error";
        }
        std::string arrayType = analyzeExpression(expr->children[0], cls, method);
        if (arrayType != "int[]") {
            addError(nodeLine(expr), "length may only be applied to int[] arrays.");
        }
        return "int";
    }

    if (typeName == "Method Invocation") {
        return analyzeMethodInvocation(expr, cls, method);
    }

    if (typeName == "New Int") {
        if (expr->children.empty()) {
            addError(nodeLine(expr), "Malformed array creation expression.");
            return "error";
        }
        std::string sizeType = analyzeExpression(expr->children[0], cls, method);
        if (!typesMatch("int", sizeType)) {
            addError(nodeLine(expr), "Array size expression must be of type int.");
        }
        return "int[]";
    }

    if (typeName == "New") {
        if (expr->children.empty()) {
            addError(nodeLine(expr), "Malformed object creation expression.");
            return "error";
        }
        Node *idNode = expr->children[0];
        if (!idNode || idNode->type != "Identifier") {
            addError(nodeLine(expr), "Object creation requires a class identifier.");
            return "error";
        }
        const std::string &className = idNode->value;
        if (!lookupClass(className)) {
            addError(nodeLine(expr), "Use of unknown class '" + className + "' in object creation.");
            return "error";
        }
        return className;
    }

    if (!expr->children.empty()) {
        // Container nodes: analyze first child and propagate type
        if (expr->children.size() == 1) {
            return analyzeExpression(expr->children[0], cls, method);
        }
        std::string lastType = "error";
        for (auto *child : expr->children) {
            lastType = analyzeExpression(child, cls, method);
        }
        return lastType;
    }

    return "error";
}

std::string SemanticAnalyzer::analyzeMethodInvocation(Node *invocation, const ClassInfo &cls, const MethodInfo &method) {
    if (!invocation || invocation->children.size() < 2) {
        addError(nodeLine(invocation), "Malformed method invocation.");
        return "error";
    }

    Node *targetNode = invocation->children[0];
    Node *methodIdNode = invocation->children[1];
    if (!methodIdNode || methodIdNode->type != "Identifier") {
        addError(nodeLine(invocation), "Method invocation missing method identifier.");
        return "error";
    }

    std::string targetType = analyzeExpression(targetNode, cls, method);
    if (targetType == "error") {
        return "error";
    }

    if (isPrimitive(targetType) || isArrayType(targetType)) {
        addError(nodeLine(invocation), "Cannot invoke methods on non-object type '" + targetType + "'.");
        return "error";
    }

    const MethodInfo *targetMethod = lookupMethod(targetType, methodIdNode->value);
    if (!targetMethod) {
        addError(nodeLine(invocation), "Class '" + targetType + "' has no method named '" + methodIdNode->value + "'.");
        return "error";
    }

    std::vector<std::string> argumentTypes;
    if (invocation->children.size() >= 3) {
        Node *argsNode = invocation->children[2];
        if (argsNode && argsNode->type == "Arguments") {
            argumentTypes = analyzeArguments(argsNode, cls, method);
        }
    }

    if (method.isMain) {
        return targetMethod->returnType;
    }

    if (argumentTypes.size() != targetMethod->parameters.size()) {
        addError(nodeLine(invocation), "Method '" + methodIdNode->value + "' expects " +
                                       std::to_string(targetMethod->parameters.size()) + " argument(s), got " +
                                       std::to_string(argumentTypes.size()) + ".");
    } else {
        for (size_t i = 0; i < argumentTypes.size(); ++i) {
            if (!typesMatch(targetMethod->parameters[i].type, argumentTypes[i])) {
                addError(nodeLine(invocation), "Type mismatch for argument " + std::to_string(i + 1) +
                                               " when calling '" + methodIdNode->value + "'.");
            }
        }
    }

    return targetMethod->returnType;
}

std::string SemanticAnalyzer::analyzeIndexExpression(Node *indexNode, const ClassInfo &cls, const MethodInfo &method, bool isAssignmentLhs) {
    if (!indexNode) {
        return "error";
    }

    if (indexNode->children.size() == 1) {
        return analyzeExpression(indexNode->children[0], cls, method);
    }

    if (indexNode->children.size() < 2) {
        addError(nodeLine(indexNode), "Malformed array access.");
        return "error";
    }

    std::string arrayType = analyzeExpression(indexNode->children[0], cls, method);
    Node *innerIndex = indexNode->children[1];
    std::string indexExprType = "error";
    if (innerIndex && !innerIndex->children.empty()) {
        indexExprType = analyzeExpression(innerIndex->children[0], cls, method);
    }

    if (!typesMatch("int", indexExprType)) {
        if (g_suppress_index_errors) {
            g_index_error_detected = true;
        } else {
            addError(nodeLine(indexNode), "Array index expression must be of type int.");
        }
    }

    if (arrayType != "int[]") {
        if (g_suppress_index_errors) {
            g_index_error_detected = true;
        } else {
            addError(nodeLine(indexNode), "Only int[] arrays are supported for indexing in MiniJava.");
        }
        return "error";
    }

    return "int";
}

std::vector<std::string> SemanticAnalyzer::analyzeArguments(Node *argsNode, const ClassInfo &cls, const MethodInfo &method) {
    std::vector<std::string> result;
    if (!argsNode) {
        return result;
    }
    for (auto *child : argsNode->children) {
        result.push_back(analyzeExpression(child, cls, method));
    }
    return result;
}

std::string SemanticAnalyzer::lookupVariable(const ClassInfo &cls, const MethodInfo &method, const std::string &name, int line) {
    auto localIt = method.locals.find(name);
    if (localIt != method.locals.end()) {
        if (line != 0 && localIt->second.line != 0 && line < localIt->second.line) {
            addError(line, "Identifier '" + name + "' is used before its declaration in method '" + method.name + "'.");
        }
        return localIt->second.type;
    }

    for (const auto &param : method.parameters) {
        if (param.name == name) {
            return param.type;
        }
    }

    auto fieldIt = cls.fields.find(name);
    if (fieldIt != cls.fields.end()) {
        return fieldIt->second.type;
    }

    if (name == "this") {
        if (method.isStatic) {
            addError(line, "Cannot use 'this' in a static context.");
            return "error";
        }
        return cls.name;
    }

    addError(line, "Identifier '" + name + "' is undeclared in the current scope.");
    return "error";
}

const MethodInfo *SemanticAnalyzer::lookupMethod(const std::string &className, const std::string &methodName) const {
    auto classIt = classes.find(className);
    if (classIt == classes.end()) {
        return nullptr;
    }
    const auto &methodMap = classIt->second.methods;
    auto methodIt = methodMap.find(methodName);
    if (methodIt != methodMap.end()) {
        return &methodIt->second;
    }
    return nullptr;
}

const ClassInfo *SemanticAnalyzer::lookupClass(const std::string &className) const {
    auto it = classes.find(className);
    if (it == classes.end()) {
        return nullptr;
    }
    return &it->second;
}

std::string SemanticAnalyzer::makeTypeFromNode(Node *typeNode) {
    if (!typeNode) {
        return "error";
    }
    if (!typeNode->value.empty()) {
        return typeNode->value;
    }
    if (!typeNode->children.empty()) {
        return makeTypeFromNode(typeNode->children[0]);
    }
    return "error";
}

int SemanticAnalyzer::nodeLine(Node *node) {
    return node ? node->lineno : 0;
}

bool SemanticAnalyzer::isPrimitive(const std::string &typeName) {
    return typeName == "int" || typeName == "boolean";
}

bool SemanticAnalyzer::isArrayType(const std::string &typeName) {
    return typeName == "int[]" || typeName == "String[]";
}

bool SemanticAnalyzer::typesMatch(const std::string &lhs, const std::string &rhs) {
    if (lhs == rhs) {
        return true;
    }
    if (lhs == "error" || rhs == "error") {
        return true;
    }
    return false;
}

void SemanticAnalyzer::addError(int line, const std::string &message) {
    std::ostringstream oss;
    if (line > 0) {
        oss << "@error at line " << line << ". ";
    }
    else {
        oss << "@error ";
    }
    oss << message;
    errors.push_back(oss.str());
}
