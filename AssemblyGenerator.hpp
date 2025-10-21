#ifndef ASSEMBLY_GENERATOR_HPP
#define ASSEMBLY_GENERATOR_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "IR.hpp"

class AssemblyGenerator {
public:
    explicit AssemblyGenerator(const ir::Module &module);

    bool writeToFile(const std::string &filename);
    const std::vector<std::string> &getErrors() const { return errors; }

private:
    const ir::Module &irModule;
    std::vector<std::string> errors;

    std::string sanitize(const std::string &name) const;
    std::string translateInstruction(const ir::Instruction &inst,
                                     const std::string &fnLabel,
                                     const std::unordered_map<std::string, int> &varOffsets,
                                     const std::unordered_set<std::string> &boolVars,
                                     const std::string &epilogueLabel) const;
};

#endif // ASSEMBLY_GENERATOR_HPP
