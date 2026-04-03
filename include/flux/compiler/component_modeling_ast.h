#ifndef FLUX_COMPONENT_MODELING_AST_H
#define FLUX_COMPONENT_MODELING_AST_H

#include "flux/compiler/ast.h"
#include <map>
#include <vector>
#include <string>
#include <memory>

namespace Flux {

// ============================================================================
// Hierarchical Design AST Nodes
// ============================================================================

// Subcircuit instance: X1 n1 n2 subckt_name params: (R=1k C=1u)
class SubcktInstanceAST : public ExprAST {
    std::string InstanceName;
    std::vector<std::string> Nodes;
    std::string SubcktName;
    std::map<std::string, std::string> Parameters;
    
public:
    SubcktInstanceAST(const std::string& name, const std::string& subckt)
        : InstanceName(name), SubcktName(subckt) {}
    
    TypedValue codegen(CodegenContext& context) override;
    
    void addNode(const std::string& node) { Nodes.push_back(node); }
    void addParam(const std::string& name, const std::string& value) { Parameters[name] = value; }
    
    const std::string& getInstanceName() const { return InstanceName; }
    const std::vector<std::string>& getNodes() const { return Nodes; }
    const std::string& getSubcktName() const { return SubcktName; }
    const std::map<std::string, std::string>& getParameters() const { return Parameters; }
};

// ============================================================================
// Verilog-A Lite AST Nodes
// ============================================================================

// Analog block: analog { ... }
class AnalogBlockAST : public ExprAST {
    std::vector<std::unique_ptr<ExprAST>> Contributors;
    std::map<std::string, std::string> Tolerances;
    
public:
    AnalogBlockAST() {}
    
    TypedValue codegen(CodegenContext& context) override;
    
    void addContributor(std::unique_ptr<ExprAST> contributor) { 
        Contributors.push_back(std::move(contributor)); 
    }
    void setTolerance(const std::string& param, const std::string& value) {
        Tolerances[param] = value;
    }
    const std::vector<std::unique_ptr<ExprAST>>& getContributors() const { return Contributors; }
};

// Contributor: V(node1, node2) <+ expression
class ContributorAST : public ExprAST {
    std::string NodeP;
    std::string NodeN;
    std::string Quantity;  // "V" or "I"
    std::unique_ptr<ExprAST> Expression;
    
public:
    ContributorAST(const std::string& nodeP, const std::string& nodeN, const std::string& qty)
        : NodeP(nodeP), NodeN(nodeN), Quantity(qty) {}
    
    TypedValue codegen(CodegenContext& context) override;
    
    void setExpression(std::unique_ptr<ExprAST> expr) { Expression = std::move(expr); }
    
    const std::string& getNodeP() const { return NodeP; }
    const std::string& getNodeN() const { return NodeN; }
    const std::string& getQuantity() const { return Quantity; }
    ExprAST* getExpression() const { return Expression.get(); }
};

// V() voltage access: V(node1, node2)
class VoltageAccessAST : public ExprAST {
    std::string NodeP;
    std::string NodeN;
    
public:
    VoltageAccessAST(const std::string& nodeP, const std::string& nodeN = "0")
        : NodeP(nodeP), NodeN(nodeN) {}
    
    TypedValue codegen(CodegenContext& context) override;
    
    const std::string& getNodeP() const { return NodeP; }
    const std::string& getNodeN() const { return NodeN; }
};

// I() current access: I(branch)
class CurrentAccessAST : public ExprAST {
    std::string Branch;
    
public:
    CurrentAccessAST(const std::string& branch) : Branch(branch) {}
    
    TypedValue codegen(CodegenContext& context) override;
    
    const std::string& getBranch() const { return Branch; }
};

// ddt() time derivative
class DdtExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Operand;
    
public:
    DdtExprAST(std::unique_ptr<ExprAST> operand) : Operand(std::move(operand)) {}
    
    TypedValue codegen(CodegenContext& context) override;
};

// idt() time integral
class IdtExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Operand;
    std::unique_ptr<ExprAST> IC;  // Initial condition (optional)
    
public:
    IdtExprAST(std::unique_ptr<ExprAST> operand, std::unique_ptr<ExprAST> ic = nullptr)
        : Operand(std::move(operand)), IC(std::move(ic)) {}
    
    TypedValue codegen(CodegenContext& context) override;
};

// ============================================================================
// Symbol Pin Mapping AST Nodes
// ============================================================================

// Symbol declaration
class SymbolDeclAST : public ExprAST {
    std::string SymbolName;
    std::vector<std::string> Pins;
    std::map<std::string, std::string> Properties;
    std::unique_ptr<ExprAST> Body;  // Symbol geometry/graphics
    
public:
    SymbolDeclAST(const std::string& name) : SymbolName(name) {}
    
    TypedValue codegen(CodegenContext& context) override;
    
    void addPin(const std::string& pin) { Pins.push_back(pin); }
    void setProperty(const std::string& key, const std::string& value) { Properties[key] = value; }
    void setBody(std::unique_ptr<ExprAST> body) { Body = std::move(body); }
    
    const std::string& getSymbolName() const { return SymbolName; }
    const std::vector<std::string>& getPins() const { return Pins; }
    const std::map<std::string, std::string>& getProperties() const { return Properties; }
};

// Pin mapping: pinmap symbol_pin -> subckt_pin
class PinMapAST : public ExprAST {
    std::string SymbolName;
    std::string SubcktName;
    std::map<std::string, std::string> Mappings;  // symbol_pin -> subckt_pin
    
public:
    PinMapAST(const std::string& symbol, const std::string& subckt)
        : SymbolName(symbol), SubcktName(subckt) {}
    
    TypedValue codegen(CodegenContext& context) override;
    
    void addMapping(const std::string& symbolPin, const std::string& subcktPin) {
        Mappings[symbolPin] = subcktPin;
    }
    
    const std::string& getSymbolName() const { return SymbolName; }
    const std::string& getSubcktName() const { return SubcktName; }
    const std::map<std::string, std::string>& getMappings() const { return Mappings; }
};

} // namespace Flux

#endif // FLUX_COMPONENT_MODELING_AST_H
