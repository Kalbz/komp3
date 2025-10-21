#ifndef SYMBOLTABLE_H
#define SYMBOLTABLE_H

#include <iostream>
#include <string>
#include <map>
#include "Record.hpp"
#include "Method.hpp"
#include "Scope.hpp"

class SymbolTable{
private:
    Scope root;
    Scope* current;

public:
    SymbolTable() : root(Scope()), current(&root) {
    }

    void enterScope(const std::string& name, const std::string& kind, const std::string& declaredType = ""){
        std::cout << "Entering new scope: " << name << std::endl;
        current = current->ensureChildScope(name, kind, declaredType);
        if (!declaredType.empty() && current->getDeclaredType().empty()) {
            current->setDeclaredType(declaredType);
        }
    }

    void exitScope(){
        std::cout << "Exiting scope: " << current->getName() << std::endl;
        if (current != &root) { // Ensure we don't go above the root
            current = current->getParentScope();
        }
    }

    Scope* getCurrentScope(){
        return this->current;
    }

    void put(std::string key, Record value){
        current->put(key, value);

    }

    Record lookup(std::string key){
        return current->lookup(key);
    }


    Record lookupParent(std::string key){
        return current->getParentScope()->lookup(key);
    }

    Record lookupCurrentScope(std::string key){
        return current->lookupCurrentScope(key);
    }

    Record lookupRoot(std::string key){
        return root.lookupCurrentScope(key);
    }

    void printTable(){
        root.printScope();
    }
    
    void resetTable(){
        root.resetScope();
        current = &root;
    }


void generateGraphviz(const std::string &filename) const {
    std::ofstream outStream(filename);
    int nodeCount = 0;
    outStream << "digraph SymbolTable {\n";
    root.generateDotContent(outStream, nodeCount);
    outStream << "}\n";
    outStream.close();
    std::cout << "Generated Graphviz file: " << filename << std::endl;
}


};
#endif
