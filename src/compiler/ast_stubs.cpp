// Stub codegen implementations for incomplete AST nodes
#include "flux/compiler/ast.h"

namespace Flux {

// ============ ExplainExprAST Stub ============
TypedValue ExplainExprAST::codegen(CodegenContext& context) {
    // Placeholder: just return 0.0
    // Full implementation would generate explanation/debug info
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

// ============ SubstituteStmtAST Stub ============
TypedValue SubstituteStmtAST::codegen(CodegenContext& context) {
    // Placeholder: just return 0.0
    // Full implementation would perform variable substitution
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

// ============ DebugStmtAST Stub ============
TypedValue DebugStmtAST::codegen(CodegenContext& context) {
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

// ============ SensitivityStmtAST Stub ============
TypedValue SensitivityStmtAST::codegen(CodegenContext& context) {
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

// ============ AskExprAST Stub ============
TypedValue AskExprAST::codegen(CodegenContext& context) {
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

} // namespace Flux
