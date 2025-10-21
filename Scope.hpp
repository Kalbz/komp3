#ifndef SCOPE_H
#define SCOPE_H

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include "Record.hpp"

class Method;

class Scope
{
private:
    std::string name;
    std::string kind;
    std::string declaredType;
    Scope *parentScope = nullptr;
    std::vector<Scope *> childScopes;
    std::map<std::string, Record*> records;
    Method* currentMethod = nullptr;

    int next = 0;

public:
    // Constructor for root scope
    Scope() {
        name = "root";
        kind = "root";
        declaredType.clear();
        std::cout << "Creating root scope." << std::endl;
    }



    // Constructor for child scope
    explicit Scope(Scope *parent, std::string name, std::string kind, std::string declaredType) : parentScope(parent), name(name), kind(kind), declaredType(declaredType) {
        std::cout << "Creating child scope. Parent: " << parent << std::endl;
    }
    // // Prevent copying
    // Scope(const Scope &) = delete;
    // Scope &operator=(const Scope &) = delete;

    // Destructor to clean up dynamically allocated child scopes
    ~Scope() {
        for (auto child : childScopes) {
            delete child;
        }
        for (auto& pair : records) {
            delete pair.second; // Delete dynamically allocated Record objects
        }
    }


    // // Function to create or return the next child scope
    // Scope *nextChild(std::string name, std::string type)
    // {
    //     Scope *nextChild;
    //     if (next >= childScopes.size())
    //     {
    //         nextChild = new Scope(this, name, type);
    //         childScopes.push_back(nextChild);
    //     }
    //     else
    //     {
    //         nextChild = childScopes[next];
    //     }
    //     next++;
    //     return nextChild;
    // }
    Scope* findChildScope(const std::string& childName, const std::string& childKind) const {
        for (auto* child : childScopes) {
            if (child->name == childName && child->kind == childKind) {
                return child;
            }
        }
        return nullptr;
    }

    Scope* ensureChildScope(const std::string& childName, const std::string& childKind, const std::string& childDeclaredType) {
        Scope* existing = findChildScope(childName, childKind);
        if (existing != nullptr) {
            if (!childDeclaredType.empty() && existing->declaredType.empty()) {
                existing->declaredType = childDeclaredType;
            }
            return existing;
        }
        Scope* newScope = new Scope(this, childName, childKind, childDeclaredType);
        childScopes.push_back(newScope);
        return newScope;
    }



    Record lookup(std::string key){
        auto it = records.find(key);
        if (it != records.end())
        {
            return *it->second;
        }
        else
        {
            if (parentScope != nullptr)
            {
                return parentScope->lookup(key);
            }
            else
            {
                throw std::runtime_error("Record not found");
            }
        }
    }

    Record lookupCurrentScope(std::string key){
        auto it = records.find(key);
        if (it != records.end())
        {
            return *it->second;
        }
        else
        {
            throw std::runtime_error("Record not found");
        }
    }

    Scope* findNearestClassScope() {
        Scope* scope = this;
        while (scope != nullptr && scope->getType() != "Class") {
            scope = scope->getParentScope();
        }
        return scope;
    }

    Scope* findNearestMethodScope() {
        Scope* scope = this;
        while (scope != nullptr && scope->getType() != "Method") {
            scope = scope->getParentScope();
        }
        return scope;
    }



    Scope* getParentScope(){
        return parentScope;
    }
    



    void resetScope(){
        next = 0;
        for (int i = 0; i < childScopes.size(); i++)
        {
            childScopes[i]->resetScope();
        }
    }

    std::string getName() const {
        return name;
    }

    std::string getType() const {
        return kind;
    }

    std::string getDeclaredType() const {
        return declaredType;
    }

    void setDeclaredType(const std::string& value) {
        declaredType = value;
    }
    
    void put(std::string key, Record value) {
        std::cout << "Adding symbol to scope: " << key << std::endl;
        records[key] = new Record(value);
    }


    void printScope(){
        std::cout << "Printing scope. Records count: " << records.size() << std::endl;
        for (auto& pair : records)
        {
            std::cout << "ID: " << pair.second->getId() << " Type: " << pair.second->getType() << std::endl;
        }
        for (int i = 0; i < childScopes.size(); i++)
        {
            childScopes[i]->printScope();
        }
    }

    // Ensure there's a way to set and get this currentMethod, either directly or through methods
    void setCurrentMethod(Method* method) {
        this->currentMethod = method;
    }

    Method* getCurrentMethod() const {
        return this->currentMethod;
    }


void generateDotContent(std::ostream &out, int &nodeCount) const {
    int currentNode = nodeCount++;
    // Print the current node with its label
        out << "n" << currentNode << " [label=\"Scope: " << this->getName() << " (kind: " << this->getType();
        if (!declaredType.empty()) {
            out << ", declared type: " << declaredType;
        }
        out << ") Records count: " << records.size() << "\"];\n";
    
    // Print records for the current scope (if you want to include record details in the graph)
    for (const auto& pair : records) {
        out << "n" << currentNode << " -> " << "r" << nodeCount << " [label=\"" << pair.first << "\"];\n";
        out << "r" << nodeCount << " [shape=record, label=\"{" << pair.first << "|ID: " << pair.second->getId() << "|Type: " << pair.second->getType() << "}\"];\n";
        nodeCount++;
    }

    // Recursively print child scopes
    for (const auto child : childScopes) {
        int childNode = nodeCount;
        child->generateDotContent(out, nodeCount);
        // Draw edge to child node
        out << "n" << currentNode << " -> n" << childNode << ";\n";
    }
}

};




#endif
