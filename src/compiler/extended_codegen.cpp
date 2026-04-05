// Extended keyword codegen implementations
#include <iostream>
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
    // Generate try-catch-finally with error handling
    // Uses a simple flag-based approach since LLVM exception handling is complex
    
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;
    
    // Create blocks
    llvm::BasicBlock* TryBB = llvm::BasicBlock::Create(Ctx, "try_block", TheFunction);
    llvm::BasicBlock* CatchBB = llvm::BasicBlock::Create(Ctx, "catch_block", TheFunction);
    llvm::BasicBlock* FinallyBB = llvm::BasicBlock::Create(Ctx, "finally_block", TheFunction);
    llvm::BasicBlock* EndBB = llvm::BasicBlock::Create(Ctx, "try_end");
    
    // Error flag to track if exception occurred
    llvm::Value* ErrorFlag = context.Builder.CreateAlloca(llvm::Type::getInt1Ty(Ctx), nullptr, "error_flag");
    context.Builder.CreateStore(llvm::ConstantInt::get(Ctx, llvm::APInt(1, 0)), ErrorFlag);
    
    // Result storage
    llvm::Value* ResultPtr = context.Builder.CreateAlloca(llvm::Type::getDoubleTy(Ctx), nullptr, "try_result");
    
    // Branch to try block
    context.Builder.CreateBr(TryBB);
    context.Builder.SetInsertPoint(TryBB);
    
    // Generate try body
    TypedValue TryResultTV = TryBody->codegen(context);
    if (!TryResultTV.Val) {
        // Error in try body - go to catch
        context.Builder.CreateStore(llvm::ConstantInt::get(Ctx, llvm::APInt(1, 1)), ErrorFlag);
        context.Builder.CreateBr(CatchBB);
    } else {
        // Store result
        context.Builder.CreateStore(TryResultTV.Val, ResultPtr);
        context.Builder.CreateBr(FinallyBB);
    }
    
    // Catch blocks
    TheFunction->insert(TheFunction->end(), CatchBB);
    context.Builder.SetInsertPoint(CatchBB);
    
    // Execute catch clauses
    TypedValue CatchResultTV = TryResultTV;
    for (auto& [VarName, Handler] : CatchClauses) {
        // Save old variable
        llvm::Value* OldVal = context.NamedValues[VarName];
        
        // Get error code (would be passed from throw in full implementation)
        llvm::Value* ErrorCode = llvm::ConstantFP::get(Ctx, llvm::APFloat(-1.0));
        context.NamedValues[VarName] = ErrorCode;
        
        // Generate handler
        TypedValue HandlerTV = Handler->codegen(context);
        if (HandlerTV.Val) {
            CatchResultTV = HandlerTV;
            context.Builder.CreateStore(HandlerTV.Val, ResultPtr);
        }
        
        // Restore variable
        context.NamedValues[VarName] = OldVal;
    }
    
    context.Builder.CreateBr(FinallyBB);
    
    // Finally block
    TheFunction->insert(TheFunction->end(), FinallyBB);
    context.Builder.SetInsertPoint(FinallyBB);
    
    if (FinallyBody) {
        FinallyBody->codegen(context);
    }
    
    context.Builder.CreateBr(EndBB);
    
    // End block - merge results
    TheFunction->insert(TheFunction->end(), EndBB);
    context.Builder.SetInsertPoint(EndBB);
    
    llvm::Value* FinalResult = context.Builder.CreateLoad(llvm::Type::getDoubleTy(Ctx), ResultPtr, "final_result");
    
    return TypedValue(FinalResult, TypeKind::Double);
}

// ============ Throw Codegen ============

TypedValue ThrowExprAST::codegen(CodegenContext& context) {
    // Generate exception value
    TypedValue ExTV = Exception->codegen(context);
    if (!ExTV.Val) return TypedValue();
    
    // Generate call to runtime error handler
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule.get();
    
    // Get or create the error function: void flux_throw_error(double code, const char* msg)
    llvm::Function* ThrowFunc = TheModule->getFunction("flux_throw_error");
    if (!ThrowFunc) {
        llvm::Type* Params[] = {
            llvm::Type::getDoubleTy(Ctx),
            llvm::PointerType::get(Ctx, 0)
        };
        llvm::FunctionType* FT = llvm::FunctionType::get(
            llvm::Type::getVoidTy(Ctx),
            Params,
            false
        );
        ThrowFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_throw_error", TheModule);
    }
    
    // Create error message
    llvm::Value* MsgPtr = context.Builder.CreateGlobalStringPtr("Runtime error", "throw_msg");
    
    // Call the throw function (this will abort execution)
    context.Builder.CreateCall(ThrowFunc, {ExTV.Val, MsgPtr});
    
    // Create unreachable after throw (no return)
    llvm::BasicBlock* UnreachableBB = llvm::BasicBlock::Create(Ctx, "after_throw", context.Builder.GetInsertBlock()->getParent());
    context.Builder.CreateBr(UnreachableBB);
    context.Builder.SetInsertPoint(UnreachableBB);
    
    // Return error code (won't be reached)
    return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(-1.0)), TypeKind::Double);
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
    // Generate corner case analysis as a switch-like structure
    // Evaluates each case expression and collects results into an array
    
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;
    size_t NumCases = Cases.size();
    
    if (NumCases == 0) {
        return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
    }
    
    // Create result array on stack: { double, double, ... } for each case
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* ResultArrayTy = llvm::ArrayType::get(DoubleTy, NumCases);
    llvm::Value* ResultArray = context.Builder.CreateAlloca(ResultArrayTy, nullptr, "corner_results");
    
    // Evaluate each case and store in result array
    for (size_t i = 0; i < NumCases; i++) {
        auto& [CaseName, CaseExpr] = Cases[i];
        
        // Generate case expression
        TypedValue CaseTV = CaseExpr->codegen(context);
        if (!CaseTV.Val) return TypedValue();
        
        // Store result in array at index i
        llvm::Value* Indices[] = {
            llvm::ConstantInt::get(Ctx, llvm::APInt(32, 0)),
            llvm::ConstantInt::get(Ctx, llvm::APInt(32, i))
        };
        llvm::Value* CasePtr = context.Builder.CreateGEP(ResultArrayTy, ResultArray, Indices, "case_" + std::to_string(i) + "_ptr");
        context.Builder.CreateStore(CaseTV.Val, CasePtr);
        
        std::cout << "[CodeGen] Corner case '" << CaseName << "' evaluated" << std::endl;
    }
    
    // Return pointer to results array
    llvm::Type* PtrTy = llvm::PointerType::get(Ctx, 0);
    return TypedValue(context.Builder.CreateBitCast(ResultArray, PtrTy), TypeKind::Double);
}

// ============ Pattern Match Codegen ============

void MatchExprAST::addArm(std::unique_ptr<ExprAST> pattern, std::unique_ptr<ExprAST> result) {
    Arms.emplace_back(std::move(pattern), std::move(result));
}

void MatchExprAST::setDefault(std::unique_ptr<ExprAST> arm) {
    DefaultArm = std::move(arm);
}

TypedValue MatchExprAST::codegen(CodegenContext& context) {
    // Generate pattern matching as a series of if-else comparisons
    // Each arm is checked in order, and the first match wins
    
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;
    
    // Generate the value to match against
    TypedValue MatchValTV = Value->codegen(context);
    if (!MatchValTV.Val) return TypedValue();
    
    // Create merge block for result
    llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(Ctx, "match_merge", TheFunction);
    llvm::PHINode* ResultPhi = context.Builder.CreatePHI(llvm::Type::getDoubleTy(Ctx), Arms.size() + 1, "match_result");
    
    llvm::BasicBlock* CurrentBB = context.Builder.GetInsertBlock();
    
    // Generate each arm
    for (size_t i = 0; i < Arms.size(); i++) {
        auto& [Pattern, Result] = Arms[i];
        
        // Generate pattern value
        TypedValue PatternTV = Pattern->codegen(context);
        if (!PatternTV.Val) return TypedValue();
        
        // Create comparison
        llvm::Value* IsMatch = context.Builder.CreateFCmpOEQ(
            MatchValTV.Val,
            PatternTV.Val,
            "match_check_" + std::to_string(i));
        
        // Create blocks for this arm
        llvm::BasicBlock* MatchBB = llvm::BasicBlock::Create(Ctx, "match_arm_" + std::to_string(i), TheFunction);
        llvm::BasicBlock* NextBB = (i == Arms.size() - 1) ? 
            (DefaultArm ? llvm::BasicBlock::Create(Ctx, "match_default", TheFunction) : MergeBB) :
            llvm::BasicBlock::Create(Ctx, "match_next_" + std::to_string(i + 1), TheFunction);
        
        context.Builder.CreateCondBr(IsMatch, MatchBB, NextBB);
        
        // Generate arm body
        context.Builder.SetInsertPoint(MatchBB);
        TypedValue ResultTV = Result->codegen(context);
        if (!ResultTV.Val) return TypedValue();
        
        ResultPhi->addIncoming(ResultTV.Val, context.Builder.GetInsertBlock());
        context.Builder.CreateBr(MergeBB);
        
        // Move to next block
        if (i < Arms.size() - 1 || DefaultArm) {
            TheFunction->insert(TheFunction->end(), NextBB);
            context.Builder.SetInsertPoint(NextBB);
        }
    }
    
    // Generate default arm if exists
    if (DefaultArm) {
        TypedValue DefaultTV = DefaultArm->codegen(context);
        if (!DefaultTV.Val) return TypedValue();
        
        ResultPhi->addIncoming(DefaultTV.Val, context.Builder.GetInsertBlock());
        context.Builder.CreateBr(MergeBB);
    } else {
        // No default - return 0.0 if no match
        ResultPhi->addIncoming(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), CurrentBB);
        context.Builder.CreateBr(MergeBB);
    }
    
    // Merge block
    TheFunction->insert(TheFunction->end(), MergeBB);
    context.Builder.SetInsertPoint(MergeBB);
    
    return TypedValue(ResultPhi, TypeKind::Double);
}

// ============ Foreach Codegen ============

TypedValue ForeachExprAST::codegen(CodegenContext& context) {
    // Generate foreach loop that iterates over a vector/collection
    // Assumes iterable is a {double*, i32} vector struct
    
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;
    
    // Generate iterable expression
    TypedValue IterableTV = Iterable->codegen(context);
    if (!IterableTV.Val) return TypedValue();
    
    // Extract pointer and size from vector struct
    // Vector struct: { double*, i32 }
    llvm::Value* DataPtr = context.Builder.CreateExtractValue(IterableTV.Val, {0}, "vec_data");
    llvm::Value* SizeVal = context.Builder.CreateExtractValue(IterableTV.Val, {1}, "vec_size");
    
    // Create loop blocks
    llvm::BasicBlock* PreheaderBB = context.Builder.GetInsertBlock();
    llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(Ctx, "foreach_loop", TheFunction);
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(Ctx, "after_foreach");
    
    // Save old variable
    llvm::Value* OldVal = context.NamedValues[VarName];
    
    // Branch to loop
    context.Builder.CreateBr(LoopBB);
    context.Builder.SetInsertPoint(LoopBB);
    
    // Create PHI for index
    llvm::PHINode* IndexPhi = context.Builder.CreatePHI(llvm::Type::getInt32Ty(Ctx), 2, "foreach_idx");
    IndexPhi->addIncoming(llvm::ConstantInt::get(Ctx, llvm::APInt(32, 0)), PreheaderBB);
    
    // Load element from array
    llvm::Value* ElemPtr = context.Builder.CreateGEP(
        llvm::Type::getDoubleTy(Ctx),
        DataPtr,
        IndexPhi,
        "elem_ptr");
    llvm::Value* ElemVal = context.Builder.CreateLoad(llvm::Type::getDoubleTy(Ctx), ElemPtr, VarName.c_str());
    
    // Set loop variable
    context.NamedValues[VarName] = ElemVal;
    
    // Generate body
    llvm::BasicBlock* ContBB = llvm::BasicBlock::Create(Ctx, "foreach_cont", TheFunction);
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = ContBB;
    
    TypedValue BodyTV = Body->codegen(context);
    
    context.Builder.CreateBr(ContBB);
    context.Builder.SetInsertPoint(ContBB);
    
    // Increment index
    llvm::Value* NextIdx = context.Builder.CreateAdd(
        IndexPhi,
        llvm::ConstantInt::get(Ctx, llvm::APInt(32, 1)),
        "next_idx");
    IndexPhi->addIncoming(NextIdx, context.Builder.GetInsertBlock());
    
    // Check loop condition
    llvm::Value* ContinueCond = context.Builder.CreateICmpSLT(NextIdx, SizeVal, "foreach_cond");
    context.Builder.CreateCondBr(ContinueCond, LoopBB, AfterBB);
    
    // Continue after loop
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);
    
    // Restore variable
    context.NamedValues[VarName] = OldVal;
    context.CurrentLoopEnd = nullptr;
    context.CurrentLoopCont = nullptr;
    
    return BodyTV ? BodyTV : TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
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
