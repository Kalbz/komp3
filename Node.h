#ifndef NODE_H
#define NODE_H

#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

class Node {
public:
    int id = 0;
    int lineno = 0;
    std::string type;
    std::string value;
    std::vector<Node *> children;

    Node(std::string t, std::string v, int l)
        : id(0), lineno(l), type(std::move(t)), value(std::move(v)) {}

    Node() : id(0), lineno(0), type("uninitialised"), value("uninitialised") {}

    void print_tree(int depth = 0) {
        for (int i = 0; i < depth; ++i) {
            std::cout << "  ";
        }
        std::cout << type << ":" << value << std::endl;
        for (auto *child : children) {
            if (child != nullptr) {
                child->print_tree(depth + 1);
            }
        }
    }

    void generate_tree() {
        std::ofstream outStream("tree.dot");
        if (!outStream.is_open()) {
            std::cerr << "Failed to open tree.dot for writing.\n";
            return;
        }

        int count = 0;
        outStream << "digraph {\n";
        generate_tree_content(count, outStream);
        outStream << "}\n";

        std::cout << "\nBuilt a parse-tree at tree.dot. Use 'make tree' to generate the pdf version.\n\n";
    }

protected:
    void generate_tree_content(int &count, std::ostream &outStream) {
        id = count++;
        outStream << "n" << id << " [label=\"" << type << ":" << value << "\"];\n";

        for (auto *child : children) {
            if (child == nullptr) {
                continue;
            }
            child->generate_tree_content(count, outStream);
            outStream << "n" << id << " -> n" << child->id << "\n";
        }
    }
};

class VarDeclaration : public Node {
public:
    using Node::Node;
};

class MethodDeclaration : public Node {
public:
    using Node::Node;
};

class ClassDeclaration : public Node {
public:
    using Node::Node;
};

class Parameter : public Node {
public:
    using Node::Node;
};

class IntExpression : public Node {
public:
    using Node::Node;
};

class BooleanExpression : public Node {
public:
    using Node::Node;
};

class EqualExpression : public Node {
public:
    using Node::Node;
};

class LessThanExpression : public Node {
public:
    using Node::Node;
};

class GreaterThanExpression : public Node {
public:
    using Node::Node;
};

class NotExpression : public Node {
public:
    using Node::Node;
};

class New : public Node {
public:
    using Node::Node;
};

class Length : public Node {
public:
    using Node::Node;
};

class Type : public Node {
public:
    using Node::Node;
};

class Identifier : public Node {
public:
    using Node::Node;
};

class Index : public Node {
public:
    using Node::Node;
};

class IntArray : public Node {
public:
    using Node::Node;
};

class BooleanFactor : public Node {
public:
    using Node::Node;
};

class IntFactor : public Node {
public:
    using Node::Node;
};

class ClassFactor : public Node {
public:
    using Node::Node;
};

class Return : public Node {
public:
    using Node::Node;
};

class Arguments : public Node {
public:
    using Node::Node;
};

class IfElseWhile : public Node {
public:
    using Node::Node;
};

class Assignment : public Node {
public:
    using Node::Node;
};

#endif // NODE_H
