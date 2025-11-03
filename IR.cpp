#include "IR.hpp"

#include <cctype>
#include <iomanip>
#include <iostream>
#include <queue>
#include <set>

using namespace std;

namespace ir {

namespace {

string escapeDotLabel(const string &input) {
    string escaped;
    escaped.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\l";
            break;
        default:
            escaped += ch;
        }
    }
    return escaped;
}

string sanitizeId(const string &input) {
    string sanitized;
    sanitized.reserve(input.size());
    for (char ch : input) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
            sanitized.push_back(ch);
        } else {
            sanitized.push_back('_');
        }
    }
    if (sanitized.empty()) {
        sanitized = "cluster";
    }
    return sanitized;
}

string instructionOperandsToString(const Instruction &inst) {
    ostringstream oss;
    if (!inst.dst.empty()) {
        oss << inst.dst << " = ";
    }
    oss << opcodeToString(inst.op);
    if (!inst.lhs.empty()) {
        oss << " " << inst.lhs;
    }
    if (!inst.rhs.empty()) {
        oss << ", " << inst.rhs;
    }
    if (!inst.extra.empty()) {
        oss << " [" << inst.extra << "]";
    }
    if (!inst.comment.empty()) {
        oss << " ; " << inst.comment;
    }
    return oss.str();
}

} // namespace

BasicBlock *Function::createBlock(const string &hint) {
    string blockName = hint;
    if (blockName.empty()) {
        blockName = "block_" + to_string(blocks.size());
    }
    auto blk = make_unique<BasicBlock>(blockName);
    auto *ptr = blk.get();
    blocks.push_back(std::move(blk));
    if (!entry) {
        entry = ptr;
    }
    IR_DEBUG_LOG("Created block " + blockName + " in function " + name);
    return ptr;
}

BasicBlock *Function::findBlock(const string &blockName) const {
    for (const auto &blk : blocks) {
        if (blk->name == blockName) {
            return blk.get();
        }
    }
    return nullptr;
}

Function *Module::createFunction(const string &qualifiedName) {
    auto fn = make_unique<Function>();
    fn->name = qualifiedName;
    auto *ptr = fn.get();
    functions.push_back(std::move(fn));
    IR_DEBUG_LOG("Created function " + qualifiedName);
    return ptr;
}

const Function *Module::findFunction(const string &qualifiedName) const {
    for (const auto &fn : functions) {
        if (fn->name == qualifiedName) {
            return fn.get();
        }
    }
    return nullptr;
}

string opcodeToString(Opcode op) {
    switch (op) {
    case Opcode::Assign:
        return "mov";
    case Opcode::Add:
        return "add";
    case Opcode::Sub:
        return "sub";
    case Opcode::Mul:
        return "mul";
    case Opcode::Div:
        return "div";
    case Opcode::LessThan:
        return "lt";
    case Opcode::GreaterThan:
        return "gt";
    case Opcode::Equal:
        return "eq";
    case Opcode::And:
        return "and";
    case Opcode::Or:
        return "or";
    case Opcode::Not:
        return "not";
    case Opcode::Neg:
        return "neg";
    case Opcode::LoadImmediate:
        return "iconst";
    case Opcode::LoadVar:
        return "load";
    case Opcode::StoreVar:
        return "store";
    case Opcode::ArrayLoad:
        return "aload";
    case Opcode::ArrayStore:
        return "astore";
    case Opcode::Length:
        return "length";
    case Opcode::Call:
        return "call";
    case Opcode::Return:
        return "return";
    case Opcode::Print:
        return "print";
    case Opcode::Label:
        return "label";
    case Opcode::Goto:
        return "goto";
    case Opcode::IfTrueGoto:
        return "if_true";
    case Opcode::IfFalseGoto:
        return "if_false";
    default:
        return "???";
    }
}

string Instruction::toString() const {
    return instructionOperandsToString(*this);
}

void dumpDot(const Module &module, ostream &out) {
    out << "digraph CFG {\n";
    out << "  graph [splines=ortho];\n";
    out << "  node [shape=box];\n";

    for (const auto &fnPtr : module.functions) {
        if (!fnPtr) {
            continue;
        }
        out << "  subgraph cluster_" << sanitizeId(fnPtr->name) << " {\n";
        out << "    label = \"" << escapeDotLabel(fnPtr->name) << "\";\n";

        for (const auto &blockPtr : fnPtr->blocks) {
            if (!blockPtr) {
                continue;
            }
            out << "    \"" << fnPtr->name << "::" << blockPtr->name << "\" [label=\""
                << escapeDotLabel(blockPtr->name + "\\l");
            for (const auto &inst : blockPtr->instructions) {
                out << escapeDotLabel(inst.toString()) << "\\l";
            }
            out << "\"];\n";
        }

        // edges for control flow.
        for (const auto &blockPtr : fnPtr->blocks) {
            if (!blockPtr) {
                continue;
            }
            const string from = fnPtr->name + "::" + blockPtr->name;
            if (blockPtr->trueExit) {
                out << "    \"" << from << "\" -> \"" << fnPtr->name
                    << "::" << blockPtr->trueExit->name << "\"";
                out << " [xlabel=\"true\"];\n";
            }
            if (blockPtr->falseExit) {
                out << "    \"" << from << "\" -> \"" << fnPtr->name
                    << "::" << blockPtr->falseExit->name << "\"";
                if (blockPtr->trueExit == blockPtr->falseExit) {
                    out << " [xlabel=\"loop\"]";
                } else {
                    out << " [xlabel=\"false\"]";
                }
                out << ";\n";
            }
        }

        out << "  }\n";
    }

    out << "}\n";
}

} // namespace ir
