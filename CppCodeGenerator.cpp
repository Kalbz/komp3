#include "CppCodeGenerator.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <set>
#include <sstream>

using namespace std;

namespace {

vector<string> splitTokens(const string &text) {
    istringstream iss(text);
    return vector<string>((istream_iterator<string>(iss)),
                          istream_iterator<string>());
}

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

} 

CppCodeGenerator::CppCodeGenerator(const SemanticAnalyzer &semanticAnalyzer,
                                   const ir::Module &module)
    : analyzer(semanticAnalyzer), irModule(module) {}

string CppCodeGenerator::sanitizeIdentifier(const string &name) const {
    string result;
    result.reserve(name.size());
    for (char ch : name) {
        if (isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
            result.push_back(ch);
        } else if (ch == ':' || ch == '.') {
            result.push_back('_');
        } else {
            result.push_back('_');
        }
    }
    if (!result.empty() && isdigit(static_cast<unsigned char>(result.front()))) {
        result = "_" + result;
    }
    return result;
}

string CppCodeGenerator::structName(const string &className) const {
    return sanitizeIdentifier(className);
}

string CppCodeGenerator::pointerType(const string &className) const {
    return structName(className) + "*";
}

bool CppCodeGenerator::isClassType(const string &type) const {
    const auto &classes = analyzer.getClasses();
    return classes.find(type) != classes.end();
}

const ClassInfo *CppCodeGenerator::findClass(const string &className) const {
    const auto &classes = analyzer.getClasses();
    auto it = classes.find(className);
    if (it == classes.end()) {
        return nullptr;
    }
    return &it->second;
}

const MethodInfo *CppCodeGenerator::findMethod(const string &className, const string &methodName) const {
    const ClassInfo *cls = findClass(className);
    if (!cls) {
        return nullptr;
    }
    auto it = cls->methods.find(methodName);
    if (it == cls->methods.end()) {
        return nullptr;
    }
    return &it->second;
}

string CppCodeGenerator::functionName(const string &className, const string &methodName) const {
    return sanitizeIdentifier(className + "__" + methodName);
}

string CppCodeGenerator::blockLabel(const string &fnPrefix, const string &blockName) const {
    return sanitizeIdentifier(fnPrefix + "_" + blockName);
}

string CppCodeGenerator::mapType(const string &type) const {
    if (type == "int") {
        return "int";
    }
    if (type == "int[]") {
        return "std::vector<int>";
    }
    if (type == "String[]") {
        return "std::vector<std::string>";
    }
    if (type == "boolean") {
        return "bool";
    }
    if (type == "void") {
        return "void";
    }
    if (isClassType(type)) {
        return pointerType(type);
    }
    // treat unknown custom types as int handles.
    return "int";
}

bool CppCodeGenerator::writeToFile(const string &filename) {
    errors.clear();
    ofstream out(filename);
    if (!out.is_open()) {
        errors.push_back("Failed to open '" + filename + "' for writing.");
        return false;
    }

    out << "#include <iostream>\n";
    out << "#include <vector>\n";
    out << "#include <string>\n";
    out << "#include <iomanip>\n\n";
    out << "using std::cout;\n";
    out << "using std::endl;\n\n";

    const auto &classes = analyzer.getClasses();
    vector<string> classOrder;
    classOrder.reserve(classes.size());
    for (const auto &entry : classes) {
        classOrder.push_back(entry.first);
    }
    sort(classOrder.begin(), classOrder.end());

    // struct definitions
    for (const auto &className : classOrder) {
        const ClassInfo &cls = classes.at(className);
        out << "struct " << structName(className) << " {\n";
        if (cls.fields.empty()) {
            out << "};\n\n";
        } else {
            vector<string> fieldNames;
            fieldNames.reserve(cls.fields.size());
            for (const auto &fieldEntry : cls.fields) {
                fieldNames.push_back(fieldEntry.first);
            }
            sort(fieldNames.begin(), fieldNames.end());
            for (const auto &fieldName : fieldNames) {
                const auto &field = cls.fields.at(fieldName);
                out << "    " << mapType(field.type) << " " << sanitizeIdentifier(field.name) << ";\n";
            }
            out << "};\n\n";
        }
    }

    auto buildMethodSignature = [&](const string &className, const MethodInfo &method) {
        string signature;
        string fnName = functionName(className, method.name);
        signature += mapType(method.returnType) + " " + fnName + "(";
        bool first = true;
        if (!method.isStatic) {
            signature += pointerType(className) + " self";
            first = false;
        }
        for (const auto &param : method.parameters) {
            if (!first) {
                signature += ", ";
            }
            first = false;
            signature += mapType(param.type) + " " + sanitizeIdentifier(param.name);
        }
        signature += ")";
        return signature;
    };

    // forward declarations (constructors + methods)
    vector<string> prototypes;
    prototypes.reserve(classOrder.size() * 2);
    for (const auto &className : classOrder) {
        const ClassInfo &cls = classes.at(className);
        prototypes.push_back(pointerType(className) + " " + functionName(className, "ctor") + "();");
        vector<string> methodNames;
        methodNames.reserve(cls.methods.size());
        for (const auto &methodEntry : cls.methods) {
            methodNames.push_back(methodEntry.first);
        }
        sort(methodNames.begin(), methodNames.end());
        for (const auto &methodName : methodNames) {
            const MethodInfo &method = cls.methods.at(methodName);
            if (method.isMain) {
                continue;
            }
            prototypes.push_back(buildMethodSignature(className, method) + ";");
        }
    }

    for (const auto &proto : prototypes) {
        out << proto << "\n";
    }
    if (!prototypes.empty()) {
        out << "\n";
    }

    // constructor definitions
    for (const auto &className : classOrder) {
        out << pointerType(className) << " " << functionName(className, "ctor") << "() {\n";
        out << "    " << pointerType(className) << " obj = new " << structName(className) << "();\n";
        const ClassInfo &cls = classes.at(className);
        if (!cls.fields.empty()) {
            vector<string> fieldNames;
            fieldNames.reserve(cls.fields.size());
            for (const auto &fieldEntry : cls.fields) {
                fieldNames.push_back(fieldEntry.first);
            }
            sort(fieldNames.begin(), fieldNames.end());
            for (const auto &fieldName : fieldNames) {
                const auto &field = cls.fields.at(fieldName);
                string target = "obj->" + sanitizeIdentifier(field.name);
                if (field.type == "boolean") {
                    out << "    " << target << " = false;\n";
                } else {
                    out << "    " << target << " = 0;\n";
                }
            }
        }
        out << "    return obj;\n";
        out << "}\n\n";
    }

    bool hasMain = false;

    for (const auto &fnPtr : irModule.functions) {
        if (!fnPtr) {
            continue;
        }
        if (!emitFunction(*fnPtr, out)) {
            errors.push_back("Failed to emit function '" + fnPtr->name + "'.");
        } else {
            if (fnPtr->name.find("::main") != string::npos) {
                hasMain = true;
            }
        }
    }

    if (!hasMain) {
        out << "int main() {\n";
        out << "    cout << \"Program contains no main method.\" << endl;\n";
        out << "    return 0;\n";
        out << "}\n";
    }

    return errors.empty();
}

bool CppCodeGenerator::emitFunction(const ir::Function &fn, ostream &out) {
    auto pos = fn.name.find("::");
    if (pos == string::npos) {
        errors.push_back("Function name '" + fn.name + "' is not qualified with class.");
        return false;
    }
    string className = fn.name.substr(0, pos);
    string methodName = fn.name.substr(pos + 2);

    const MethodInfo *method = findMethod(className, methodName);
    if (!method) {
        errors.push_back("Unknown method '" + methodName + "' in class '" + className + "'.");
        return false;
    }

    string fnPrefix = sanitizeIdentifier(fn.name);
    bool isMain = method->isMain;

    string selfParamName;
    unordered_map<string, string> aliases;
    unordered_map<string, string> varTypes;
    unordered_map<string, string> varClass;
    unordered_set<string> boolTemps;
    set<string> declared;

    auto registerVar = [&](const string &rawName, const string &cppType) {
        string name = sanitizeIdentifier(rawName);
        varTypes[name] = cppType;
    };
    auto registerClass = [&](const string &rawName, const string &cls) {
        string name = sanitizeIdentifier(rawName);
        varClass[name] = cls;
    };
    auto registerBool = [&](const string &rawName) {
        string name = sanitizeIdentifier(rawName);
        boolTemps.insert(name);
    };

    vector<pair<string, string>> localsToDeclare;

    if (isMain) {
        out << "int main() {\n";
    } else {
        string signature = mapType(method->returnType) + " " + functionName(className, methodName) + "(";
        bool first = true;
        if (!method->isStatic) {
            selfParamName = sanitizeIdentifier(className + "_self");
            signature += pointerType(className) + " " + selfParamName;
            first = false;
            declared.insert(selfParamName);
            registerVar(selfParamName, pointerType(className));
            registerClass(selfParamName, className);
            aliases["this"] = selfParamName;
        }
        for (const auto &param : method->parameters) {
            string paramName = sanitizeIdentifier(param.name);
            string cppType = mapType(param.type);
            if (!first) {
                signature += ", ";
            }
            first = false;
            signature += cppType + " " + paramName;
            declared.insert(paramName);
            registerVar(paramName, cppType);
            if (param.type == "boolean") {
                registerBool(paramName);
            }
            if (isClassType(param.type)) {
                registerClass(paramName, param.type);
            }
        }
        signature += ")";
        out << signature << " {\n";
    }

    if (isMain) {
        for (const auto &param : method->parameters) {
            string paramName = sanitizeIdentifier(param.name);
            string cppType = mapType(param.type);
            declared.insert(paramName);
            registerVar(paramName, cppType);
            if (param.type == "boolean") {
                registerBool(paramName);
            }
            if (isClassType(param.type)) {
                registerClass(paramName, param.type);
            }
            localsToDeclare.emplace_back(paramName, cppType);
        }
    }

    vector<string> localNames;
    localNames.reserve(method->locals.size());
    for (const auto &entry : method->locals) {
        localNames.push_back(entry.first);
    }
    sort(localNames.begin(), localNames.end());
    for (const auto &name : localNames) {
        const auto &local = method->locals.at(name);
        string varName = sanitizeIdentifier(local.name);
        string cppType = mapType(local.type);
        localsToDeclare.emplace_back(varName, cppType);
        declared.insert(varName);
        registerVar(varName, cppType);
        if (local.type == "boolean") {
            registerBool(varName);
        }
        if (isClassType(local.type)) {
            registerClass(varName, local.type);
        }
    }

    auto resolveAlias = [&](const string &raw) -> string {
        if (raw.empty()) {
            return string();
        }
        auto it = aliases.find(raw);
        if (it != aliases.end()) {
            return it->second;
        }
        return sanitizeIdentifier(raw);
    };

    set<string> temporaries;
    unordered_map<string, string> tempTypes;

    auto markBool = [&](const string &rawName) {
        if (rawName.empty()) {
            return;
        }
        string name = sanitizeIdentifier(rawName);
        boolTemps.insert(name);
        tempTypes[name] = "bool";
    };

    for (const auto &blockPtr : fn.blocks) {
        if (!blockPtr) {
            continue;
        }
        for (const auto &inst : blockPtr->instructions) {
            string dstName = sanitizeIdentifier(inst.dst);
            string lhsName = resolveAlias(inst.lhs);
            switch (inst.op) {
            case ir::Opcode::LessThan:
            case ir::Opcode::GreaterThan:
            case ir::Opcode::Equal:
            case ir::Opcode::And:
            case ir::Opcode::Or:
            case ir::Opcode::Not:
                markBool(inst.dst);
                break;
            case ir::Opcode::Add:
            case ir::Opcode::Sub:
            case ir::Opcode::Mul:
            case ir::Opcode::Div:
            case ir::Opcode::ArrayLoad:
            case ir::Opcode::Length:
                if (!inst.dst.empty()) {
                    tempTypes[dstName] = "int";
                }
                break;
            case ir::Opcode::Assign: {
                if (!inst.dst.empty()) {
                    if (boolTemps.find(lhsName) != boolTemps.end()) {
                        markBool(inst.dst);
                    }
                    auto clsIt = varClass.find(lhsName);
                    if (clsIt != varClass.end()) {
                        varClass[dstName] = clsIt->second;
                        tempTypes[dstName] = pointerType(clsIt->second);
                    }
                }
                break;
            }
            case ir::Opcode::Call: {
                string dst = sanitizeIdentifier(inst.dst);
                if (inst.lhs == "new_int_array") {
                    tempTypes[dst] = "std::vector<int>";
                    break;
                }
                if (inst.extra == "ctor") {
                    varClass[dst] = inst.lhs;
                    tempTypes[dst] = pointerType(inst.lhs);
                    break;
                }
                vector<string> tokens = splitTokens(inst.extra);
                if (tokens.empty()) {
                    break;
                }
                string methodToken = tokens.front();
                string targetName = resolveAlias(inst.lhs);
                string owningClass;
                auto vcIt = varClass.find(targetName);
                if (vcIt != varClass.end()) {
                    owningClass = vcIt->second;
                }
                if (owningClass.empty()) {
                    owningClass = className;
                }
                const MethodInfo *callee = findMethod(owningClass, methodToken);
                if (callee && !dst.empty()) {
                    if (callee->returnType == "boolean") {
                        markBool(dst);
                    } else if (isClassType(callee->returnType)) {
                        varClass[dst] = callee->returnType;
                        tempTypes[dst] = pointerType(callee->returnType);
                    } else if (callee->returnType == "int") {
                        tempTypes[dst] = "int";
                    } else if (callee->returnType == "int[]") {
                        tempTypes[dst] = "std::vector<int>";
                    } else if (callee->returnType != "void") {
                        tempTypes[dst] = mapType(callee->returnType);
                    }
                }
                break;
            }
            default:
                break;
            }
        }
    }

    auto considerName = [&](const string &raw) {
        if (raw.empty() || isNumber(raw)) {
            return;
        }
        if (aliases.find(raw) != aliases.end()) {
            return;
        }
        if (raw == "ctor") {
            return;
        }
        string name = sanitizeIdentifier(raw);
        if (declared.find(name) == declared.end()) {
            temporaries.insert(name);
        }
    };

    for (const auto &blockPtr : fn.blocks) {
        if (!blockPtr) {
            continue;
        }
        for (const auto &inst : blockPtr->instructions) {
            considerName(inst.dst);
            if (inst.op == ir::Opcode::ArrayStore) {
                considerName(inst.dst);
                considerName(inst.lhs);
                considerName(inst.rhs);
            } else {
                if (!(inst.op == ir::Opcode::Call && inst.extra == "ctor")) {
                    considerName(inst.lhs);
                }
                considerName(inst.rhs);
            }
        }
    }

    unordered_set<string> declaredInBody;
    auto emitDeclaration = [&](const string &name) {
        const string &type = varTypes[name];
        out << "    " << type << " " << name;
        if (type == "bool") {
            out << " = false;\n";
        } else if (type == "int") {
            out << " = 0;\n";
        } else if (type == "std::vector<int>" || type == "std::vector<std::string>") {
            out << ";\n";
        } else if (!type.empty() && type.back() == '*') {
            out << " = nullptr;\n";
        } else {
            out << " = 0;\n";
        }
    };

    for (const auto &decl : localsToDeclare) {
        if (!declaredInBody.insert(decl.first).second) {
            continue;
        }
        emitDeclaration(decl.first);
    }

    vector<string> tempList(temporaries.begin(), temporaries.end());
    sort(tempList.begin(), tempList.end());
    for (const auto &tmp : tempList) {
        string type;
        if (varTypes.find(tmp) != varTypes.end()) {
            type = varTypes[tmp];
        } else if (tempTypes.find(tmp) != tempTypes.end()) {
            type = tempTypes[tmp];
            varTypes[tmp] = type;
        } else if (varClass.find(tmp) != varClass.end()) {
            type = pointerType(varClass.at(tmp));
            varTypes[tmp] = type;
        } else if (boolTemps.find(tmp) != boolTemps.end()) {
            type = "bool";
            varTypes[tmp] = type;
        } else {
            type = "int";
            varTypes[tmp] = type;
        }
        if (type == "bool") {
            boolTemps.insert(tmp);
        }
        if (!declaredInBody.insert(tmp).second) {
            continue;
        }
        emitDeclaration(tmp);
    }

    out << "    cout << std::boolalpha;\n";

    if (!fn.entry && !fn.blocks.empty()) {
        const_cast<ir::Function &>(fn).entry = fn.blocks.front().get();
    }

    if (fn.entry) {
        out << "    goto " << blockLabel(fnPrefix, fn.entry->name) << ";\n";
    }

    unordered_map<string, string> labelCache;

    for (const auto &blockPtr : fn.blocks) {
        if (!blockPtr) {
            continue;
        }
        string label = blockLabel(fnPrefix, blockPtr->name);
        labelCache[blockPtr->name] = label;
        out << "\n" << label << ":\n";
        for (const auto &inst : blockPtr->instructions) {
            string stmt = translateInstruction(inst, fnPrefix, labelCache, boolTemps, aliases, varClass);
            if (!stmt.empty()) {
                out << "    " << stmt << "\n";
            }
        }
    }

    if (isMain) {
        out << "    return 0;\n";
    }

    out << "}\n\n";
    return true;
}

string CppCodeGenerator::translateInstruction(const ir::Instruction &inst,
                                              const string &fnPrefix,
                                              unordered_map<string, string> &labelCache,
                                              const unordered_set<string> &boolTemps,
                                              const unordered_map<string, string> &aliases,
                                              const unordered_map<string, string> &varClass) const {
    auto labelFor = [&](const string &target) -> string {
        if (target.empty()) {
            return "";
        }
        auto it = labelCache.find(target);
        if (it != labelCache.end()) {
            return it->second;
        }
        string sanitized = blockLabel(fnPrefix, target);
        labelCache[target] = sanitized;
        return sanitized;
    };

    auto resolveAlias = [&](const string &raw) -> string {
        if (raw.empty()) {
            return string();
        }
        auto it = aliases.find(raw);
        if (it != aliases.end()) {
            return it->second;
        }
        return sanitizeIdentifier(raw);
    };

    auto formatOperand = [&](const string &operand) -> string {
        if (operand.empty()) {
            return operand;
        }
        if (isNumber(operand)) {
            return operand;
        }
        return resolveAlias(operand);
    };

    switch (inst.op) {
    case ir::Opcode::Assign:
        return sanitizeIdentifier(inst.dst) + " = " + formatOperand(inst.lhs) + ";";
    case ir::Opcode::Add:
        return sanitizeIdentifier(inst.dst) + " = " + formatOperand(inst.lhs) + " + " + formatOperand(inst.rhs) + ";";
    case ir::Opcode::Sub:
        return sanitizeIdentifier(inst.dst) + " = " + formatOperand(inst.lhs) + " - " + formatOperand(inst.rhs) + ";";
    case ir::Opcode::Mul:
        return sanitizeIdentifier(inst.dst) + " = " + formatOperand(inst.lhs) + " * " + formatOperand(inst.rhs) + ";";
    case ir::Opcode::Div:
        return sanitizeIdentifier(inst.dst) + " = " + formatOperand(inst.lhs) + " / " + formatOperand(inst.rhs) + ";";
    case ir::Opcode::LessThan:
        return sanitizeIdentifier(inst.dst) + " = (" + formatOperand(inst.lhs) + " < " + formatOperand(inst.rhs) + ");";
    case ir::Opcode::GreaterThan:
        return sanitizeIdentifier(inst.dst) + " = (" + formatOperand(inst.lhs) + " > " + formatOperand(inst.rhs) + ");";
    case ir::Opcode::Equal:
        return sanitizeIdentifier(inst.dst) + " = (" + formatOperand(inst.lhs) + " == " + formatOperand(inst.rhs) + ");";
    case ir::Opcode::And:
        return sanitizeIdentifier(inst.dst) + " = (" + formatOperand(inst.lhs) + " && " + formatOperand(inst.rhs) + ");";
    case ir::Opcode::Or:
        return sanitizeIdentifier(inst.dst) + " = (" + formatOperand(inst.lhs) + " || " + formatOperand(inst.rhs) + ");";
    case ir::Opcode::Not:
        return sanitizeIdentifier(inst.dst) + " = (!" + formatOperand(inst.lhs) + ");";
    case ir::Opcode::Neg:
        return sanitizeIdentifier(inst.dst) + " = (-" + formatOperand(inst.lhs) + ");";
    case ir::Opcode::LoadImmediate: {
        string dst = sanitizeIdentifier(inst.dst);
        if (boolTemps.find(dst) != boolTemps.end()) {
            return dst + string(" = ") + ((inst.lhs == "0") ? "false" : "true") + ";";
        }
        return dst + " = " + formatOperand(inst.lhs) + ";";
    }
    case ir::Opcode::Print:
        return "cout << " + formatOperand(inst.lhs) + " << endl;";
    case ir::Opcode::Return:
        if (inst.lhs.empty()) {
            return "return;";
        }
        return "return " + formatOperand(inst.lhs) + ";";
    case ir::Opcode::Goto:
        return "goto " + labelFor(inst.extra) + ";";
    case ir::Opcode::IfTrueGoto:
        return "if (" + formatOperand(inst.lhs) + ") goto " + labelFor(inst.extra) + ";";
    case ir::Opcode::IfFalseGoto:
        return "if (!(" + formatOperand(inst.lhs) + ")) goto " + labelFor(inst.extra) + ";";
    case ir::Opcode::ArrayLoad:
        return sanitizeIdentifier(inst.dst) + " = " + formatOperand(inst.lhs) + "[" + formatOperand(inst.rhs) + "];";
    case ir::Opcode::ArrayStore:
        return sanitizeIdentifier(inst.dst) + "[" + formatOperand(inst.lhs) + "] = " + formatOperand(inst.rhs) + ";";
    case ir::Opcode::Length:
        return sanitizeIdentifier(inst.dst) + " = " + formatOperand(inst.lhs) + ".size();";
    case ir::Opcode::Call: {
        if (inst.lhs == "new_int_array") {
            string dst = sanitizeIdentifier(inst.dst);
            string length = formatOperand(inst.rhs);
            return dst + " = std::vector<int>(" + length + ");";
        }
        if (inst.extra == "ctor") {
            string dst = sanitizeIdentifier(inst.dst);
            string ctor = functionName(inst.lhs, "ctor");
            return dst + " = " + ctor + "();";
        }
        vector<string> tokens = splitTokens(inst.extra);
        if (tokens.empty()) {
            return "/* call */;";
        }
        string methodToken = tokens.front();
        tokens.erase(tokens.begin());
        string targetExpr = formatOperand(inst.lhs);
        string targetName = resolveAlias(inst.lhs);
        string owningClass;
        auto clsIt = varClass.find(targetName);
        if (clsIt != varClass.end()) {
            owningClass = clsIt->second;
        }
        if (owningClass.empty()) {
            owningClass = inst.lhs;
        }
        string calleeName = owningClass.empty() ? sanitizeIdentifier(methodToken)
                                                : functionName(owningClass, methodToken);
        stringstream ss;
        if (!inst.dst.empty()) {
            ss << sanitizeIdentifier(inst.dst) << " = ";
        }
        ss << calleeName << "(" << targetExpr;
        for (const auto &arg : tokens) {
            ss << ", " << formatOperand(arg);
        }
        ss << ");";
        return ss.str();
    }
    case ir::Opcode::Label:
        return "";
    default:
        return "/* unsupported opcode */";
    }
}
