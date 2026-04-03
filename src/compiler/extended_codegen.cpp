// Extended keyword codegen implementations
#include "flux/compiler/ast.h"

namespace Flux {

// ============ Try/Catch Codegen ============

void TryCatchExprAST::addCatch(const std::string& var, std::unique_ptr<ExprAST> handler) {
    CatchClauses.emplace_back(var, std::move(handler));
}

void TryCatchExprAST::setFinally(std::unique_ptr<ExprAST> body) {
    FinallyBody = std::move(body);
}

TypedValue TryCatchExprAST::codegen(CodegenContext& context) {
    // Generate try body
    TypedValue ResultTV = TryBody->codegen(context);
    if (!ResultTV.Val) return TypedValue();
    
    // Catch clauses would generate exception handling code
    // For now, just return the try body result
    return ResultTV;
}

// ============ Throw Codegen ============

TypedValue ThrowExprAST::codegen(CodegenContext& context) {
    // Generate exception value
    TypedValue ExTV = Exception->codegen(context);
    if (!ExTV.Val) return TypedValue();
    
    // In full implementation, this would generate LLVM exception handling
    // For now, just return 0.0
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

// ============ Assert Codegen ============

TypedValue AssertExprAST::codegen(CodegenContext& context) {
    // Generate condition check
    TypedValue CondTV = Condition->codegen(context);
    if (!CondTV.Val) return TypedValue();
    
    // Create assertion check
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* ContinueBB = llvm::BasicBlock::Create(context.TheContext, "assert_continue", TheFunction);
    llvm::BasicBlock* FailBB = llvm::BasicBlock::Create(context.TheContext, "assert_fail", TheFunction);
    
    llvm::Value* IsTrue = context.Builder.CreateFCmpONE(
        CondTV.Val,
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)),
        "assert_check");
    
    context.Builder.CreateCondBr(IsTrue, ContinueBB, FailBB);
    
    // Fail block - would print error message
    context.Builder.SetInsertPoint(FailBB);
    llvm::Value* MsgPtr = context.Builder.CreateGlobalStringPtr(
        Message.empty() ? "Assertion failed!" : Message, "assert_msg");
    llvm::Function* PrintF = context.TheModule->getFunction("println_string");
    if (PrintF) {
        context.Builder.CreateCall(PrintF, {MsgPtr});
    }
    context.Builder.CreateBr(ContinueBB);
    
    // Continue block
    context.Builder.SetInsertPoint(ContinueBB);
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
}

// ============ Yield Codegen ============

TypedValue YieldExprAST::codegen(CodegenContext& context) {
    // Generate yielded value
    TypedValue ValueTV = Value->codegen(context);
    if (!ValueTV.Val) return TypedValue();
    
    // In full implementation, this would save state and return from generator
    // For now, just return the value
    return ValueTV;
}

// ============ Corner Case Codegen ============

void CornerExprAST::addCase(const std::string& name, std::unique_ptr<ExprAST> expr) {
    Cases.emplace_back(name, std::move(expr));
}

TypedValue CornerExprAST::codegen(CodegenContext& context) {
    // Generate corner case analysis
    // For now, return 0.0
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

// ============ Pattern Match Codegen ============

void MatchExprAST::addArm(std::unique_ptr<ExprAST> pattern, std::unique_ptr<ExprAST> result) {
    Arms.emplace_back(std::move(pattern), std::move(result));
}

void MatchExprAST::setDefault(std::unique_ptr<ExprAST> arm) {
    DefaultArm = std::move(arm);
}

TypedValue MatchExprAST::codegen(CodegenContext& context) {
    // Generate pattern matching
    // For now, return 0.0
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

// ============ Foreach Codegen ============

TypedValue ForeachExprAST::codegen(CodegenContext& context) {
    // Generate foreach loop
    // For now, return 0.0
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

// ============ Repeat-Until Codegen ============

TypedValue RepeatUntilExprAST::codegen(CodegenContext& context) {
    // Generate repeat-until loop (do-while style)
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* BodyBB = llvm::BasicBlock::Create(context.TheContext, "repeat_body", TheFunction);
    llvm::BasicBlock* CondBB = llvm::BasicBlock::Create(context.TheContext, "until_cond");
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(context.TheContext, "after_repeat");
    
    // Set up break/continue targets
    llvm::BasicBlock* OldLoopEnd = context.CurrentLoopEnd;
    llvm::BasicBlock* OldLoopCont = context.CurrentLoopCont;
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = CondBB;
    
    context.Builder.CreateBr(BodyBB);
    
    // Body
    context.Builder.SetInsertPoint(BodyBB);
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) return TypedValue();
    context.Builder.CreateBr(CondBB);
    
    // Condition
    TheFunction->insert(TheFunction->end(), CondBB);
    context.Builder.SetInsertPoint(CondBB);
    TypedValue CondTV = Condition->codegen(context);
    if (!CondTV.Val) return TypedValue();
    
    llvm::Value* IsDone = context.Builder.CreateFCmpONE(
        CondTV.Val,
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)),
        "until_check");
    
    context.Builder.CreateCondBr(IsDone, AfterBB, BodyBB);
    
    // After
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);
    
    // Restore break/continue targets
    context.CurrentLoopEnd = OldLoopEnd;
    context.CurrentLoopCont = OldLoopCont;
    
    return BodyTV;
}

} // namespace Flux
