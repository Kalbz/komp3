#include "IRGenerator.hpp"

#include <sstream>
#include <unordered_set>

using namespace std;

namespace {
constexpr const char *INDENT = "  ";

bool isNodeType(const Node *node, const string &type) {
    return node && node->type == type;
}

bool isNodeValue(const Node *node, const string &value) {
    return node && node->value == value;
}

string nodeLocation(const Node *node) {
    if (!node) {
        return "<unknown location>";
    }
    ostringstream oss;
    oss << "line " << node->lineno << " (" << node->type << ")";
    return oss.str();
}
} // namespace

IRGenerator::IRGenerator(const SemanticAnalyzer &analyzer)
    : semanticAnalyzer(analyzer) {}

string IRGenerator::nextTemp(const string &hint) {
    return hint + to_string(tempCounter++);
}

string IRGenerator::nextBlockName(const string &hint) {
    ostringstream oss;
    oss << hint << "_" << blockCounter++;
    return oss.str();
}

void IRGenerator::addError(const Node *node, const string &message) {
    ostringstream oss;
    oss << nodeLocation(node) << ": " << message;
    errors.push_back(oss.str());
}

void IRGenerator::addError(int line, const string &message) {
    ostringstream oss;
    oss << "line " << line << ": " << message;
    errors.push_back(oss.str());
}

void IRGenerator::addError(const string &message) {
    errors.push_back(message);
}

void IRGenerator::ensureCurrentBlock() {
    if (!currentFunction) {
        addError("Internal error: attempt to emit without active function.");
        return;
    }
    if (!currentBlock) {
        currentBlock = currentFunction->createBlock(nextBlockName("block"));
    }
}

const ir::Module &IRGenerator::generate() {
    module.functions.clear();
    errors.clear();
    tempCounter = 0;
    blockCounter = 0;
    currentFunction = nullptr;
    currentBlock = nullptr;

    const auto &classes = semanticAnalyzer.getClasses();
    if (classes.empty()) {
        addError("No classes found during IR generation.");
        return module;
    }

    for (const auto &entry : classes) {
        generateForClass(entry.second);
    }

    return module;
}

void IRGenerator::generateForClass(const ClassInfo &cls) {
    IR_DEBUG_LOG("Generating IR for class " << cls.name);
    for (const auto &methodPair : cls.methods) {
        generateForMethod(cls, methodPair.second);
    }
}

void IRGenerator::generateForMethod(const ClassInfo &cls, const MethodInfo &method) {
    if (!method.body) {
        addError(method.line, "Method '" + method.name + "' in class '" + cls.name + "' has no body to translate.");
        return;
    }

    string qualifiedName = cls.name + "::" + method.name;
    currentFunction = module.createFunction(qualifiedName);
    currentFunction->parameters.clear();
    currentFunction->locals.clear();
    currentFunction->parameters.reserve(method.parameters.size());
    for (const auto &param : method.parameters) {
        currentFunction->parameters.push_back(param.name);
    }
    for (const auto &localEntry : method.locals) {
        currentFunction->locals.push_back(localEntry.first);
    }

    tempCounter = 0;
    blockCounter = 0;
    currentBlock = currentFunction->createBlock(nextBlockName("entry"));

    IR_DEBUG_LOG("Entering function " << qualifiedName);
    generateBody(method.body);

    // ensure functions that have no explicit return end with one.
    if (!method.isMain && method.returnType != "void") {
        bool hasReturn = false;
        for (const auto &blockPtr : currentFunction->blocks) {
            for (const auto &inst : blockPtr->instructions) {
                if (inst.op == ir::Opcode::Return) {
                    hasReturn = true;
                    break;
                }
            }
            if (hasReturn) {
                break;
            }
        }
        if (!hasReturn) {
            addError(method.body, "Missing return statement in method '" + method.name + "'.");
        }
    }

    currentFunction = nullptr;
    currentBlock = nullptr;
}

void IRGenerator::generateBody(Node *bodyNode) {
    if (!bodyNode) {
        return;
    }
    for (auto *child : bodyNode->children) {
        if (!child) {
            continue;
        }
        if (child->type == "Body") {
            generateStatementList(child);
        } else if (child->type == "Return") {
            generateReturn(child);
        } else if (child->type == "VarDeclaration") {
            // locals already collected; no IR emitted.
            IR_DEBUG_LOG("Skipping VarDeclaration in body for IR.");
        } else {
            generateStatement(child);
        }
    }
}

void IRGenerator::generateStatementList(Node *listNode) {
    for (auto *child : listNode->children) {
        if (!child) {
            continue;
        }
        generateStatement(child);
    }
}

void IRGenerator::generateStatement(Node *statementNode) {
    if (!statementNode) {
        return;
    }

    if (statementNode->type == "Statements" || statementNode->type == "Body") {
        generateStatementList(statementNode);
        return;
    }

    if (statementNode->type == "Return") {
        generateReturn(statementNode);
        return;
    }

    if (statementNode->type == "Assignment") {
        generateAssignment(statementNode);
        return;
    }

    if (statementNode->type == "Print") {
        generatePrint(statementNode);
        return;
    }

    if (statementNode->type == "IF" || statementNode->type == "IF ELSE") {
        generateIf(statementNode);
        return;
    }

    if (statementNode->type == "WHILE") {
        generateWhile(statementNode);
        return;
    }

    if (statementNode->type == "EmptyStateMentList") {
        return;
    }

    // attempt to interpret as nested statement container.
    for (auto *child : statementNode->children) {
        generateStatement(child);
    }
}

void IRGenerator::generateReturn(Node *node) {
    ensureCurrentBlock();
    if (!node || node->children.empty()) {
        currentBlock->instructions.emplace_back(ir::Opcode::Return, "", "", "", "", "return void");
        return;
    }
    string value = generateExpression(node->children.front());
    currentBlock->instructions.emplace_back(ir::Opcode::Return, "", value);
}

void IRGenerator::generatePrint(Node *node) {
    ensureCurrentBlock();
    if (!node || node->children.empty()) {
        addError(node, "print statement missing expression.");
        return;
    }
    string value = generateExpression(node->children.front());
    currentBlock->instructions.emplace_back(ir::Opcode::Print, "", value);
}

void IRGenerator::generateAssignment(Node *node) {
    ensureCurrentBlock();
    if (!node || node->children.size() < 2) {
        addError(node, "assignment node malformed.");
        return;
    }
    Node *lhs = node->children[0];
    Node *rhs = node->children[1];

    if (lhs->type == "Index") {
        if (lhs->children.size() < 2) {
            addError(lhs, "array assignment missing components.");
            return;
        }
        string arrayRef = generateExpression(lhs->children[0]);
        string indexRef = generateExpression(lhs->children[1]);
        string value = generateExpression(rhs);
        currentBlock->instructions.emplace_back(ir::Opcode::ArrayStore, arrayRef, indexRef, value);
    } else {
        string target = generateIdentifier(lhs);
        string value = generateExpression(rhs);
        currentBlock->instructions.emplace_back(ir::Opcode::Assign, target, value);
    }
}

void IRGenerator::generateIf(Node *node) {
    ensureCurrentBlock();
    if (!node || node->children.empty()) {
        addError(node, "if statement missing condition.");
        return;
    }

    string condTemp = generateExpression(node->children[0]);

    ir::BasicBlock *thenBlock = currentFunction->createBlock(nextBlockName("then"));
    ir::BasicBlock *elseBlock = nullptr;
    ir::BasicBlock *joinBlock = currentFunction->createBlock(nextBlockName("endif"));

    Node *thenNode = node->children.size() > 1 ? node->children[1] : nullptr;
    Node *elseNode = node->children.size() > 2 ? node->children[2] : nullptr;

    if (elseNode) {
        elseBlock = currentFunction->createBlock(nextBlockName("else"));
    }

    currentBlock->instructions.emplace_back(ir::Opcode::IfTrueGoto, "", condTemp, "", thenBlock->name);
    if (elseBlock) {
        currentBlock->instructions.emplace_back(ir::Opcode::IfFalseGoto, "", condTemp, "", elseBlock->name);
        currentBlock->falseExit = elseBlock;
    } else {
        currentBlock->instructions.emplace_back(ir::Opcode::IfFalseGoto, "", condTemp, "", joinBlock->name);
        currentBlock->falseExit = joinBlock;
    }
    currentBlock->trueExit = thenBlock;

    // then branch
    currentBlock = thenBlock;
    if (thenNode) {
        generateStatementList(thenNode);
    }
    currentBlock->instructions.emplace_back(ir::Opcode::Goto, "", "", "", joinBlock->name);
    currentBlock->trueExit = joinBlock;

    // else branch if exists
    if (elseBlock) {
        currentBlock = elseBlock;
        generateStatementList(elseNode);
        currentBlock->instructions.emplace_back(ir::Opcode::Goto, "", "", "", joinBlock->name);
        currentBlock->trueExit = joinBlock;
    }

    currentBlock = joinBlock;
}

void IRGenerator::generateWhile(Node *node) {
    ensureCurrentBlock();
    if (!node || node->children.size() < 2) {
        addError(node, "while statement malformed.");
        return;
    }

    ir::BasicBlock *conditionBlock = currentFunction->createBlock(nextBlockName("while_cond"));
    ir::BasicBlock *bodyBlock = currentFunction->createBlock(nextBlockName("while_body"));
    ir::BasicBlock *exitBlock = currentFunction->createBlock(nextBlockName("while_exit"));

    currentBlock->instructions.emplace_back(ir::Opcode::Goto, "", "", "", conditionBlock->name);
    currentBlock->trueExit = conditionBlock;

    // condition evaluation
    currentBlock = conditionBlock;
    string condTemp = generateExpression(node->children[0]);
    currentBlock->instructions.emplace_back(ir::Opcode::IfTrueGoto, "", condTemp, "", bodyBlock->name);
    currentBlock->instructions.emplace_back(ir::Opcode::IfFalseGoto, "", condTemp, "", exitBlock->name);
    currentBlock->trueExit = bodyBlock;
    currentBlock->falseExit = exitBlock;

    // body
    currentBlock = bodyBlock;
    generateStatementList(node->children[1]);
    currentBlock->instructions.emplace_back(ir::Opcode::Goto, "", "", "", conditionBlock->name);
    currentBlock->trueExit = conditionBlock;

    // exit
    currentBlock = exitBlock;
}

std::string IRGenerator::generateIndex(Node *node) {
    if (!node || node->children.size() < 2) {
        addError(node, "index expression malformed.");
        return "<invalid-index>";
    }
    Node *baseNode = node->children[0];
    Node *indexNode = node->children[1];

    string base = generateExpression(baseNode);
    string idx = generateExpression(indexNode);
    string tmp = nextTemp("elem");
    ensureCurrentBlock();
    currentBlock->instructions.emplace_back(ir::Opcode::ArrayLoad, tmp, base, idx);
    return tmp;
}

std::string IRGenerator::generateBinary(const std::string &opName, ir::Opcode opCode, Node *node) {
    if (!node || node->children.size() < 2) {
        addError(node, opName + " expression malformed.");
        return "<invalid>";
    }
    string lhsValue = generateExpression(node->children[0]);
    string rhsValue = generateExpression(node->children[1]);
    string temp = nextTemp("tmp");
    ensureCurrentBlock();
    currentBlock->instructions.emplace_back(opCode, temp, lhsValue, rhsValue);
    return temp;
}

std::string IRGenerator::generateUnary(ir::Opcode opCode, Node *node) {
    if (!node || node->children.empty()) {
        addError(node, "unary expression malformed.");
        return "<invalid>";
    }
    string operand = generateExpression(node->children[0]);
    string temp = nextTemp("tmp");
    ensureCurrentBlock();
    currentBlock->instructions.emplace_back(opCode, temp, operand);
    return temp;
}

std::string IRGenerator::generateIdentifier(Node *node) {
    if (!node) {
        addError("Null identifier node encountered.");
        return "<invalid-id>";
    }

    if (node->type == "Identifier") {
        return node->value;
    }

    // some parser nodes wrap identifiers in lists/containers.
    if (!node->children.empty()) {
        return generateIdentifier(node->children[0]);
    }

    addError(node, "Unsupported identifier node.");
    return "<invalid-id>";
}

std::string IRGenerator::generateLiteral(Node *node) {
    if (!node) {
        return "<invalid-literal>";
    }

    if (node->type == "Int") {
        string temp = nextTemp("const");
        ensureCurrentBlock();
        currentBlock->instructions.emplace_back(ir::Opcode::LoadImmediate, temp, node->value);
        return temp;
    }

    if (node->type == "Boolean") {
        string temp = nextTemp("bool");
        ensureCurrentBlock();
        string numeric = (node->value == "true" || node->value == "TRUE") ? "1" : "0";
        currentBlock->instructions.emplace_back(ir::Opcode::LoadImmediate, temp, numeric);
        return temp;
    }

    if (node->type == "Class") {
        // 'this' keyword
        return "this";
    }

    return "<unsupported-literal>";
}

std::string IRGenerator::generateMethodInvocation(Node *node) {
    if (!node || node->children.size() < 2) {
        addError(node, "method invocation malformed.");
        return "<invalid-call>";
    }

    Node *targetNode = node->children[0];
    Node *identifierNode = node->children[1];
    Node *argsNode = node->children.size() > 2 ? node->children[2] : nullptr;

    string target = generateExpression(targetNode);
    string methodName = generateIdentifier(identifierNode);

    vector<string> arguments;
    if (argsNode) {
        for (auto *argChild : argsNode->children) {
            arguments.push_back(generateExpression(argChild));
        }
    }

    string result = nextTemp("call");
    ensureCurrentBlock();
    ostringstream meta;
    meta << methodName;
    for (const auto &arg : arguments) {
        meta << " " << arg;
    }
    currentBlock->instructions.emplace_back(ir::Opcode::Call, result, target, "", meta.str());
    return result;
}

std::string IRGenerator::generateExpression(Node *expr) {
    if (!expr) {
        addError("Null expression encountered.");
        return "<invalid-expr>";
    }

    const string &type = expr->type;
    if (type == "Identifier") {
        return generateIdentifier(expr);
    }

    if (type == "Int" || type == "Boolean" || type == "Class") {
        return generateLiteral(expr);
    }

    if (type == "AddExpression") {
        return generateBinary("add", ir::Opcode::Add, expr);
    }
    if (type == "SubExpression") {
        return generateBinary("sub", ir::Opcode::Sub, expr);
    }
    if (type == "MultExpression") {
        return generateBinary("mul", ir::Opcode::Mul, expr);
    }
    if (type == "DivExpression") {
        return generateBinary("div", ir::Opcode::Div, expr);
    }
    if (type == "AndOPExpression") {
        return generateBinary("and", ir::Opcode::And, expr);
    }
    if (type == "OrOPExpression") {
        return generateBinary("or", ir::Opcode::Or, expr);
    }
    if (type == "LChevExpression") {
        return generateBinary("lt", ir::Opcode::LessThan, expr);
    }
    if (type == "RChevExpression") {
        return generateBinary("gt", ir::Opcode::GreaterThan, expr);
    }
    if (type == "EqOpExpression") {
        return generateBinary("eq", ir::Opcode::Equal, expr);
    }
    if (type == "Not Equal") {
        return generateUnary(ir::Opcode::Not, expr);
    }
    if (type == "Index") {
        return generateIndex(expr);
    }
    if (type == "ExpressionDotLength") {
        string base = generateExpression(expr->children[0]);
        string temp = nextTemp("len");
        ensureCurrentBlock();
        currentBlock->instructions.emplace_back(ir::Opcode::Length, temp, base);
        return temp;
    }
    if (type == "Method Invocation") {
        return generateMethodInvocation(expr);
    }
    if (type == "New") {
        string temp = nextTemp("obj");
        ensureCurrentBlock();
        string className = generateIdentifier(expr->children[0]);
        currentBlock->instructions.emplace_back(ir::Opcode::Call, temp, className, "", "ctor");
        return temp;
    }
    if (type == "New Int") {
        // new int[expr]
        string length = generateExpression(expr->children[0]);
        string temp = nextTemp("arr");
        ensureCurrentBlock();
        currentBlock->instructions.emplace_back(ir::Opcode::Call, temp, "new_int_array", length);
        return temp;
    }

    if (type == "VarDeclaration") {
        if (!expr->children.empty()) {
            return generateExpression(expr->children.back());
        }
        addError(expr, "var declaration used as expression.");
        return "<invalid-var-decl>";
    }

    if (type == "IntFactor" || type == "BooleanFactor") {
        return generateLiteral(expr);
    }

    if (!expr->children.empty()) {
        // attempt to evaluate nested expression.
        return generateExpression(expr->children.front());
    }

    addError(expr, "Unsupported expression type '" + type + "'.");
    return "<unsupported-expr>";
}
