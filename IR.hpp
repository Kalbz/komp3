#ifndef IR_HPP
#define IR_HPP

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// simple debugging macro that can be enabled from the build system.
#ifndef DEBUG_IR
#define DEBUG_IR 0
#endif

#if DEBUG_IR
#define IR_DEBUG_LOG(expr)                                                            \
    do {                                                                               \
        std::ostringstream _ir_dbg_builder;                                            \
        _ir_dbg_builder << expr;                                                       \
        std::cerr << "[IR] " << _ir_dbg_builder.str() << std::endl;                    \
    } while (false)
#else
#define IR_DEBUG_LOG(expr) do { (void)0; } while (false)
#endif

namespace ir {

enum class Opcode {
    Assign,
    Add,
    Sub,
    Mul,
    Div,
    LessThan,
    GreaterThan,
    Equal,
    And,
    Or,
    Not,
    Neg,
    LoadImmediate,
    LoadVar,
    StoreVar,
    ArrayLoad,
    ArrayStore,
    Length,
    Call,
    Return,
    Print,
    Label,
    Goto,
    IfTrueGoto,
    IfFalseGoto,
};

struct Instruction {
    Opcode op = Opcode::Assign;
    std::string dst;
    std::string lhs;
    std::string rhs;
    std::string extra;
    std::string comment;

    Instruction() = default;

    Instruction(Opcode opcode,
                std::string destination,
                std::string left = {},
                std::string right = {},
                std::string meta = {},
                std::string dbgComment = {})
        : op(opcode),
          dst(std::move(destination)),
          lhs(std::move(left)),
          rhs(std::move(right)),
          extra(std::move(meta)),
          comment(std::move(dbgComment)) {}

    std::string toString() const;
};

struct BasicBlock {
    std::string name;
    std::vector<Instruction> instructions;
    BasicBlock *trueExit = nullptr;
    BasicBlock *falseExit = nullptr;

    explicit BasicBlock(std::string blockName) : name(std::move(blockName)) {}

    bool hasConditionalSuccessors() const {
        return trueExit || falseExit;
    }
};

struct Function {
    std::string name;
    std::vector<std::string> parameters;
    std::vector<std::string> locals;
    std::vector<std::unique_ptr<BasicBlock>> blocks;
    BasicBlock *entry = nullptr;

    BasicBlock *createBlock(const std::string &hint);
    BasicBlock *findBlock(const std::string &blockName) const;
};

struct Module {
    std::vector<std::unique_ptr<Function>> functions;

    Function *createFunction(const std::string &qualifiedName);
    const Function *findFunction(const std::string &qualifiedName) const;
};

std::string opcodeToString(Opcode op);
void dumpDot(const Module &module, std::ostream &out);

} // namespace ir

#endif // IR_HPP
