#include "AssemblyGenerator.hpp"

#include <cctype>
#include <fstream>
#include <set>
#include <sstream>

using namespace std;

namespace {

bool isNumber(const string &value) {
    if (value.empty()) {
        return false;
    }
    size_t idx = 0;
    if (value[0] == '-' || value[0] == '+') {
        idx = 1;
    }
    for (; idx < value.size(); ++idx) {
        if (!isdigit(static_cast<unsigned char>(value[idx]))) {
            return false;
        }
    }
    return true;
}

} // namespace

AssemblyGenerator::AssemblyGenerator(const ir::Module &module)
    : irModule(module) {}

string AssemblyGenerator::sanitize(const string &name) const {
    string result;
    result.reserve(name.size());
    for (char ch : name) {
        if (isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
            result.push_back(ch);
        } else {
            result.push_back('_');
        }
    }
    if (!result.empty() && isdigit(static_cast<unsigned char>(result.front()))) {
        result = "_" + result;
    }
    return result;
}

bool AssemblyGenerator::writeToFile(const string &filename) {
    errors.clear();
    ofstream out(filename);
    if (!out.is_open()) {
        errors.push_back("Failed to open '" + filename + "' for writing.");
        return false;
    }

    out << ".section .rodata\n";
    out << ".LC_FMT_INT:\n    .string \"%d\\n\"\n";
    out << ".LC_FMT_BOOL:\n    .string \"%s\\n\"\n";
    out << ".LC_BOOL_TRUE:\n    .string \"true\"\n";
    out << ".LC_BOOL_FALSE:\n    .string \"false\"\n\n";

    out << ".text\n";
    out << ".extern printf\n\n";

    for (const auto &fnPtr : irModule.functions) {
        if (!fnPtr) {
            continue;
        }

        string fnLabel = sanitize(fnPtr->name);

        // Collect variables and detect booleans
        set<string> variables;
        unordered_set<string> boolVars;
        auto considerVar = [&](const string &raw) {
            if (raw.empty() || isNumber(raw) || raw == "this") {
                return;
            }
            variables.insert(sanitize(raw));
        };

        for (const auto &blockPtr : fnPtr->blocks) {
            if (!blockPtr) {
                continue;
            }
            for (const auto &inst : blockPtr->instructions) {
                if (!inst.dst.empty()) {
                    considerVar(inst.dst);
                    if (inst.dst.rfind("bool", 0) == 0) {
                        boolVars.insert(sanitize(inst.dst));
                    }
                }
                considerVar(inst.lhs);
                considerVar(inst.rhs);

                switch (inst.op) {
                case ir::Opcode::LessThan:
                case ir::Opcode::GreaterThan:
                case ir::Opcode::Equal:
                case ir::Opcode::And:
                case ir::Opcode::Or:
                case ir::Opcode::Not:
                    if (!inst.dst.empty()) {
                        boolVars.insert(sanitize(inst.dst));
                    }
                    break;
                default:
                    break;
                }
            }
        }

        vector<string> orderedVars(variables.begin(), variables.end());
        unordered_map<string, int> offsets;
        int offset = -4;
        for (const auto &name : orderedVars) {
            offsets[name] = offset;
            offset -= 4;
        }

        int stackSize = static_cast<int>(orderedVars.size()) * 4;
        stackSize = (stackSize + 15) / 16 * 16;

        string epilogueLabel = ".L" + fnLabel + "_epilogue";

        out << ".globl " << fnLabel << "\n";
        out << fnLabel << ":\n";
        out << "    pushq %rbp\n";
        out << "    movq %rsp, %rbp\n";
        if (stackSize > 0) {
            out << "    subq $" << stackSize << ", %rsp\n";
        }

        for (const auto &blockPtr : fnPtr->blocks) {
            if (!blockPtr) {
                continue;
            }
            string blockLabel = ".L" + sanitize(fnLabel + "_" + blockPtr->name);
            out << "\n" << blockLabel << ":\n";
            for (const auto &inst : blockPtr->instructions) {
                out << translateInstruction(inst, fnLabel, offsets, boolVars, epilogueLabel);
            }
        }

        out << epilogueLabel << ":\n";
        out << "    leave\n";
        out << "    ret\n\n";
    }

    return errors.empty();
}

string AssemblyGenerator::translateInstruction(const ir::Instruction &inst,
                                               const string &fnLabel,
                                               const unordered_map<string, int> &offsets,
                                               const unordered_set<string> &boolVars,
                                               const string &epilogueLabel) const {
    static int boolPrintCounter = 0;

    auto mem = [&](const string &name) -> string {
        auto it = offsets.find(sanitize(name));
        if (it == offsets.end()) {
            return {};
        }
        ostringstream addr;
        addr << it->second << "(%rbp)";
        return addr.str();
    };

    auto loadValue = [&](const string &operand, const string &reg) {
        ostringstream code;
        if (isNumber(operand)) {
            code << "    movl $" << operand << ", " << reg << "\n";
        } else {
            code << "    movl " << mem(operand) << ", " << reg << "\n";
        }
        return code.str();
    };

    auto loadBool = [&](const string &operand, const string &reg32, const string &reg8) {
        ostringstream code;
        code << loadValue(operand, reg32);
        code << "    cmpl $0, " << reg32 << "\n";
        code << "    setne " << reg8 << "\n";
        code << "    movzbl " << reg8 << ", " << reg32 << "\n";
        return code.str();
    };

    auto storeResult = [&](const string &dest) {
        ostringstream code;
        code << "    movl %eax, " << mem(dest) << "\n";
        return code.str();
    };

    ostringstream oss;
    oss << "    # " << inst.toString() << "\n";

    switch (inst.op) {
    case ir::Opcode::Assign:
        oss << loadValue(inst.lhs, "%eax");
        oss << storeResult(inst.dst);
        break;
    case ir::Opcode::LoadImmediate:
        oss << "    movl $" << inst.lhs << ", %eax\n";
        oss << storeResult(inst.dst);
        break;
    case ir::Opcode::Add:
        oss << loadValue(inst.lhs, "%eax");
        if (isNumber(inst.rhs)) {
            oss << "    addl $" << inst.rhs << ", %eax\n";
        } else {
            oss << "    addl " << mem(inst.rhs) << ", %eax\n";
        }
        oss << storeResult(inst.dst);
        break;
    case ir::Opcode::Sub:
        oss << loadValue(inst.lhs, "%eax");
        if (isNumber(inst.rhs)) {
            oss << "    subl $" << inst.rhs << ", %eax\n";
        } else {
            oss << "    subl " << mem(inst.rhs) << ", %eax\n";
        }
        oss << storeResult(inst.dst);
        break;
    case ir::Opcode::Mul:
        oss << loadValue(inst.lhs, "%eax");
        if (isNumber(inst.rhs)) {
            oss << "    imull $" << inst.rhs << ", %eax\n";
        } else {
            oss << "    imull " << mem(inst.rhs) << ", %eax\n";
        }
        oss << storeResult(inst.dst);
        break;
    case ir::Opcode::Div:
        oss << loadValue(inst.lhs, "%eax");
        if (isNumber(inst.rhs)) {
            oss << "    movl $" << inst.rhs << ", %ecx\n";
        } else {
            oss << "    movl " << mem(inst.rhs) << ", %ecx\n";
        }
        oss << "    cltd\n";
        oss << "    idivl %ecx\n";
        oss << storeResult(inst.dst);
        break;
    case ir::Opcode::LessThan:
    case ir::Opcode::GreaterThan:
    case ir::Opcode::Equal:
        oss << loadValue(inst.lhs, "%eax");
        if (isNumber(inst.rhs)) {
            oss << "    cmpl $" << inst.rhs << ", %eax\n";
        } else {
            oss << "    cmpl " << mem(inst.rhs) << ", %eax\n";
        }
        if (inst.op == ir::Opcode::LessThan) {
            oss << "    setl %al\n";
        } else if (inst.op == ir::Opcode::GreaterThan) {
            oss << "    setg %al\n";
        } else {
            oss << "    sete %al\n";
        }
        oss << "    movzbl %al, %eax\n";
        oss << storeResult(inst.dst);
        break;
    case ir::Opcode::And:
        oss << loadBool(inst.lhs, "%eax", "%al");
        oss << loadBool(inst.rhs, "%ecx", "%cl");
        oss << "    andl %ecx, %eax\n";
        oss << storeResult(inst.dst);
        break;
    case ir::Opcode::Or:
        oss << loadBool(inst.lhs, "%eax", "%al");
        oss << loadBool(inst.rhs, "%ecx", "%cl");
        oss << "    orl %ecx, %eax\n";
        oss << storeResult(inst.dst);
        break;
    case ir::Opcode::Not:
        oss << loadBool(inst.lhs, "%eax", "%al");
        oss << "    xorl $1, %al\n";
        oss << "    movzbl %al, %eax\n";
        oss << storeResult(inst.dst);
        break;
    case ir::Opcode::Goto:
        oss << "    jmp .L" << sanitize(fnLabel + "_" + inst.extra) << "\n";
        break;
    case ir::Opcode::IfTrueGoto:
        if (isNumber(inst.lhs)) {
            oss << "    movl $" << inst.lhs << ", %eax\n";
            oss << "    cmpl $0, %eax\n";
        } else {
            oss << "    cmpl $0, " << mem(inst.lhs) << "\n";
        }
        oss << "    jne .L" << sanitize(fnLabel + "_" + inst.extra) << "\n";
        break;
    case ir::Opcode::IfFalseGoto:
        if (isNumber(inst.lhs)) {
            oss << "    movl $" << inst.lhs << ", %eax\n";
            oss << "    cmpl $0, %eax\n";
        } else {
            oss << "    cmpl $0, " << mem(inst.lhs) << "\n";
        }
        oss << "    je .L" << sanitize(fnLabel + "_" + inst.extra) << "\n";
        break;
    case ir::Opcode::Print: {
        string operandName = sanitize(inst.lhs);
        if (boolVars.find(operandName) != boolVars.end()) {
            int localId = boolPrintCounter++;
            string trueLabel = ".L" + sanitize(fnLabel + "_bool_true_" + to_string(localId));
            string endLabel = ".L" + sanitize(fnLabel + "_bool_end_" + to_string(localId));
            oss << "    cmpl $0, " << mem(inst.lhs) << "\n";
            oss << "    jne " << trueLabel << "\n";
            oss << "    movq $.LC_BOOL_FALSE, %rsi\n";
            oss << "    jmp " << endLabel << "\n";
            oss << trueLabel << ":\n";
            oss << "    movq $.LC_BOOL_TRUE, %rsi\n";
            oss << endLabel << ":\n";
            oss << "    movq $.LC_FMT_BOOL, %rdi\n";
            oss << "    movl $0, %eax\n";
            oss << "    call printf\n";
        } else {
            if (isNumber(inst.lhs)) {
                oss << "    movl $" << inst.lhs << ", %esi\n";
            } else {
                oss << "    movl " << mem(inst.lhs) << ", %esi\n";
            }
            oss << "    movq $.LC_FMT_INT, %rdi\n";
            oss << "    movl $0, %eax\n";
            oss << "    call printf\n";
        }
        break;
    }
    case ir::Opcode::Return:
        if (!inst.lhs.empty()) {
            if (isNumber(inst.lhs)) {
                oss << "    movl $" << inst.lhs << ", %eax\n";
            } else {
                oss << "    movl " << mem(inst.lhs) << ", %eax\n";
            }
        }
        oss << "    jmp " << epilogueLabel << "\n";
        break;
    default:
        oss << "    # unsupported opcode\n";
        break;
    }

    return oss.str();
}

