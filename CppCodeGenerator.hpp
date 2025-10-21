#ifndef CPP_CODE_GENERATOR_HPP
#define CPP_CODE_GENERATOR_HPP

#include <iosfwd>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "IR.hpp"
#include "SemanticAnalyzer.hpp"

class CppCodeGenerator {
public:
    CppCodeGenerator(const SemanticAnalyzer &semanticAnalyzer,
                     const ir::Module &module);

    bool writeToFile(const std::string &filename);
    const std::vector<std::string> &getErrors() const { return errors; }

private:
    const SemanticAnalyzer &analyzer;
    const ir::Module &irModule;
    std::vector<std::string> errors;

    std::string sanitizeIdentifier(const std::string &name) const;
    std::string structName(const std::string &className) const;
    std::string pointerType(const std::string &className) const;
    std::string mapType(const std::string &type) const;
    std::string blockLabel(const std::string &fnPrefix, const std::string &blockName) const;
    std::string functionName(const std::string &className, const std::string &methodName) const;
    bool isClassType(const std::string &type) const;
    const ClassInfo *findClass(const std::string &className) const;
    const MethodInfo *findMethod(const std::string &className, const std::string &methodName) const;

    bool emitFunction(const ir::Function &fn, std::ostream &out);
    std::string translateInstruction(const ir::Instruction &inst,
                                     const std::string &fnPrefix,
                                     std::unordered_map<std::string, std::string> &labelCache,
                                     const std::unordered_set<std::string> &boolTemps,
                                     const std::unordered_map<std::string, std::string> &aliases,
                                     const std::unordered_map<std::string, std::string> &varClass) const;
};

#endif // CPP_CODE_GENERATOR_HPP
