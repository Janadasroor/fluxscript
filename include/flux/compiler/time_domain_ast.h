#ifndef FLUX_TIME_DOMAIN_AST_H
#define FLUX_TIME_DOMAIN_AST_H

#include "flux/compiler/ast.h"
#include "flux/runtime/time_domain_api.h"
#include <string>
#include <vector>
#include <memory>

namespace Flux {

// B-source declaration: bsource name(output_node, ref_node) update(t, inputs)
class BSourceDeclAST : public ExprAST {
    std::string Name;
    std::string OutputNode;
    std::string RefNode;
    std::vector<std::string> InputNodes;
    std::unique_ptr<ExprAST> Body;  // Expression that computes output voltage
    
public:
    BSourceDeclAST(const std::string& name, const std::string& outNode, const std::string& refNode)
        : Name(name), OutputNode(outNode), RefNode(refNode) {}
    
    TypedValue codegen(CodegenContext& context) override;
    
    const std::string& getName() const { return Name; }
    const std::string& getOutputNode() const { return OutputNode; }
    const std::string& getRefNode() const { return RefNode; }
    void addInputNode(const std::string& node) { InputNodes.push_back(node); }
    void setBody(std::unique_ptr<ExprAST> body) { Body = std::move(body); }
};

// Time variable access: time
class TimeExprAST : public ExprAST {
public:
    TimeExprAST() {}
    TypedValue codegen(CodegenContext& context) override;
};

// Timestep access: dt
class TimestepExprAST : public ExprAST {
public:
    TimestepExprAST() {}
    TypedValue codegen(CodegenContext& context) override;
};

// Temperature access: temp
class TempExprAST : public ExprAST {
public:
    TempExprAST() {}
    TypedValue codegen(CodegenContext& context) override;
};

// Inputs dictionary access: inputs["node_name"]
class InputsExprAST : public ExprAST {
    std::string NodeName;
public:
    InputsExprAST(const std::string& node) : NodeName(node) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getNodeName() const { return NodeName; }
};

// Outputs dictionary: outputs["node_name"] = value
class OutputsExprAST : public ExprAST {
    std::string NodeName;
    std::unique_ptr<ExprAST> Value;
public:
    OutputsExprAST(const std::string& node, std::unique_ptr<ExprAST> val)
        : NodeName(node), Value(std::move(val)) {}
    TypedValue codegen(CodegenContext& context) override;
};

// Initial condition: initial node = value
class InitialCondAST : public ExprAST {
    std::string Node;
    std::unique_ptr<ExprAST> Value;
public:
    InitialCondAST(const std::string& node, std::unique_ptr<ExprAST> val)
        : Node(node), Value(std::move(val)) {}
    TypedValue codegen(CodegenContext& context) override;
};

// Transient analysis directive: transient step stop [start [maxstep]]
class TransientAnalysisAST : public ExprAST {
    double Timestep;
    double StopTime;
    double StartTime;
    double MaxTimestep;
    
public:
    TransientAnalysisAST(double step, double stop)
        : Timestep(step), StopTime(stop), StartTime(0.0), MaxTimestep(0.0) {}
    
    TypedValue codegen(CodegenContext& context) override;
    
    double getTimestep() const { return Timestep; }
    double getStopTime() const { return StopTime; }
    double getStartTime() const { return StartTime; }
    double getMaxTimestep() const { return MaxTimestep; }
};

} // namespace Flux

#endif // FLUX_TIME_DOMAIN_AST_H
