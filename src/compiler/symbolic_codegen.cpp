#include "flux/compiler/symbolic_ast.h"
#include "flux/runtime/symbolic_engine.h"
#include <iostream>
#include <sstream>

namespace Flux {

// ============ Symbolic AST Codegen ============

TypedValue SymDeclAST::codegen(CodegenContext& context) {
    auto& symEngine = SymbolicEngine::instance();
    symEngine.sym(VarName);
    
    std::cout << "[CodeGen] Symbolic variable '" << VarName << "' registered" << std::endl;
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
}

TypedValue SolveExprAST::codegen(CodegenContext& context) {
    auto& symEngine = SymbolicEngine::instance();
    auto solutions = symEngine.solve(LHS, RHS, Variable);
    
    std::cout << "[CodeGen] Solutions: ";
    for (size_t i = 0; i < solutions.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << solutions[i];
    }
    std::cout << std::endl;
    
    if (!solutions.empty()) {
        return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(solutions[0])), TypeKind::Double);
    }
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

TypedValue SimplifyExprAST::codegen(CodegenContext& context) {
    auto& symEngine = SymbolicEngine::instance();
    auto simplified = symEngine.simplify(Expr);
    
    std::cout << "[CodeGen] Simplified: " << simplified->toString() << std::endl;
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
}

TypedValue DifferentiateExprAST::codegen(CodegenContext& context) {
    auto& symEngine = SymbolicEngine::instance();
    auto deriv = symEngine.differentiate(Expr, Variable);
    
    std::cout << "[CodeGen] Derivative d/d" << Variable << " [" << Expr->toString() << "] = " 
              << deriv->toString() << std::endl;
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
}

TypedValue SubstituteExprAST::codegen(CodegenContext& context) {
    auto& symEngine = SymbolicEngine::instance();
    auto result = symEngine.substitute(Expr, Values);
    
    std::cout << "[CodeGen] Substituted: " << Expr->toString() << " -> " << result->toString() << std::endl;
    
    if (result->type == SymbolicExpr::Number) {
        return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(result->value)), TypeKind::Double);
    }
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

} // namespace Flux
